// =============================================================================
// Eclipse32 - RTL8139 Fast Ethernet NIC Driver
//
// The RTL8139 is QEMU's default/easy emulated NIC (-device rtl8139) and is
// one of the simplest real Ethernet chips to drive: a flat ring buffer for
// RX, four fixed TX descriptor slots for TX, no descriptor lists to manage.
// Reference: the chip's own datasheet register layout (well known/public).
// =============================================================================
#include "rtl8139.h"
#include "../pci/pci.h"
#include "../../mm/pmm.h"
#include "../../arch/x86/idt.h"
#include "../../arch/x86/pic.h"

#define RTL8139_VENDOR_ID  0x10EC
#define RTL8139_DEVICE_ID  0x8139

// ---- Registers (offsets from the I/O base BAR) ----
#define REG_MAC0       0x00   // 6 bytes, our MAC address
#define REG_MAR0       0x08   // multicast filter
#define REG_TXSTATUS0  0x10   // 4 x 4 bytes, one per TX descriptor
#define REG_TXADDR0    0x20   // 4 x 4 bytes, physical addr of each TX buffer
#define REG_RBSTART    0x30   // physical addr of RX ring buffer
#define REG_CMD        0x37
#define REG_CAPR       0x38   // current address of packet read (driver-controlled)
#define REG_CBR        0x3A   // current buffer address (chip-controlled, read-only)
#define REG_IMR        0x3C   // interrupt mask
#define REG_ISR        0x3E   // interrupt status
#define REG_TCR        0x40   // transmit config
#define REG_RCR        0x44   // receive config
#define REG_CONFIG1    0x52

// ---- Command register bits ----
#define CMD_RX_ENABLE   0x08
#define CMD_TX_ENABLE   0x04
#define CMD_RESET       0x10

// ---- Interrupt bits (ISR/IMR) ----
#define INT_ROK   0x0001   // receive OK
#define INT_RER   0x0002   // receive error
#define INT_TOK   0x0004   // transmit OK
#define INT_TER   0x0008   // transmit error

// ---- RCR bits ----
#define RCR_AAP    (1 << 0)   // accept all packets (promiscuous)
#define RCR_APM    (1 << 1)   // accept physical match (our MAC)
#define RCR_AM     (1 << 2)   // accept multicast
#define RCR_AB     (1 << 3)   // accept broadcast
#define RCR_WRAP   (1 << 7)   // don't wrap rx buffer across the end (keeps a full packet contiguous)

// RX buffer: 8KB ring + 16 bytes slack + 1500 bytes so the chip's hardware
// "WRAP" guarantee (it may write up to 1500 bytes past the nominal end when
// WRAP is set) never walks off our allocation.
#define RX_BUFFER_SIZE   (8192 + 16 + 1500)
#define RX_BUFFER_PAGES  ((RX_BUFFER_SIZE + 4095) / 4096)

#define TX_BUFFER_SIZE   2048   // generous; max Ethernet frame is 1514 bytes
#define TX_DESC_COUNT    4

typedef struct {
    bool       found;
    uint16_t   io_base;
    uint8_t    irq_line;
    mac_addr_t mac;

    uint8_t   *rx_buffer;       // physical == virtual (identity mapped low mem)
    uint32_t   rx_offset;       // our read cursor within the rx ring

    uint8_t   *tx_buffer[TX_DESC_COUNT];
    int        tx_next;         // next descriptor slot to use for sending
} rtl8139_state_t;

static rtl8139_state_t g_nic;

// -----------------------------------------------------------------------------
// IRQ handler — RTL8139 shares whatever legacy IRQ line PCI assigned it.
// We just ack the interrupt; actual frame processing happens in rtl8139_poll(),
// called regularly from net_poll(), so we don't need a queue here.
// -----------------------------------------------------------------------------
static void rtl8139_irq_handler(void *regs) {
    (void)regs;
    if (!g_nic.found) return;

    uint16_t isr = inw(g_nic.io_base + REG_ISR);
    // Write back to ack/clear the bits we observed.
    outw(g_nic.io_base + REG_ISR, isr);

    pic_send_eoi(g_nic.irq_line);
}

bool rtl8139_present(void) {
    return g_nic.found;
}

mac_addr_t rtl8139_get_mac(void) {
    return g_nic.mac;
}

bool rtl8139_init(void) {
    g_nic.found = false;

    pci_device_t *dev = pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    if (!dev) return false;

    pci_enable_device(dev);

    // RTL8139 exposes its registers via BAR0 (I/O space). Some configs also
    // expose BAR1 (MMIO); we use the I/O BAR since it's universally supported.
    uint32_t io_base = pci_bar_io_base(dev, 0);
    if (io_base == 0) return false;
    g_nic.io_base = (uint16_t)io_base;

    // ---- Power on (some real hardware needs this; harmless under QEMU) ----
    outb(g_nic.io_base + REG_CONFIG1, 0x00);

    // ---- Software reset ----
    outb(g_nic.io_base + REG_CMD, CMD_RESET);
    {
        uint32_t timeout = 1000000;
        while ((inb(g_nic.io_base + REG_CMD) & CMD_RESET) && --timeout) {
            /* spin */
        }
        if (timeout == 0) return false; // reset never completed
    }

    // ---- Read our burned-in MAC address ----
    for (int i = 0; i < MAC_ADDR_LEN; i++) {
        g_nic.mac.b[i] = inb(g_nic.io_base + REG_MAC0 + i);
    }

    // ---- Allocate and register the RX ring buffer ----
    uint32_t rx_phys = pmm_alloc_pages(RX_BUFFER_PAGES);
    if (!rx_phys) return false;
    g_nic.rx_buffer = (uint8_t *)rx_phys;
    g_nic.rx_offset = 0;
    outl(g_nic.io_base + REG_RBSTART, rx_phys);

    // ---- Allocate TX buffers (4 fixed descriptor slots) ----
    for (int i = 0; i < TX_DESC_COUNT; i++) {
        uint32_t tx_phys = pmm_alloc_pages((TX_BUFFER_SIZE + 4095) / 4096);
        if (!tx_phys) return false;
        g_nic.tx_buffer[i] = (uint8_t *)tx_phys;
        outl(g_nic.io_base + REG_TXADDR0 + i * 4, tx_phys);
    }
    g_nic.tx_next = 0;

    // ---- Interrupt mask: tell us about RX ok/err and TX ok/err ----
    outw(g_nic.io_base + REG_IMR, INT_ROK | INT_RER | INT_TOK | INT_TER);

    // ---- Receive config: accept broadcast + multicast + our own MAC,
    //      and keep packets from wrapping across the ring boundary. ----
    outl(g_nic.io_base + REG_RCR, RCR_AAP | RCR_APM | RCR_AM | RCR_AB | RCR_WRAP);

    // ---- Enable RX and TX ----
    outb(g_nic.io_base + REG_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

    // ---- Hook the IRQ line PCI gave us. PIC was remapped to 0x20 base in
    //      kmain, so IRQ N maps to vector 0x20+N. ----
    if (dev->irq_line != 0 && dev->irq_line != 0xFF) {
        g_nic.irq_line = dev->irq_line;
        idt_register_handler((uint8_t)(0x20 + dev->irq_line), rtl8139_irq_handler);
        pic_unmask_irq(dev->irq_line);
    }

    g_nic.found = true;
    return true;
}

bool rtl8139_send(const void *frame, uint16_t len) {
    if (!g_nic.found) return false;
    if (len > TX_BUFFER_SIZE) return false;

    int slot = g_nic.tx_next;
    g_nic.tx_next = (g_nic.tx_next + 1) % TX_DESC_COUNT;

    // Ethernet minimum frame size is 60 bytes (64 with CRC) — pad with zeros
    // if the caller gave us something shorter (e.g. a bare ARP request).
    uint8_t *buf = g_nic.tx_buffer[slot];
    for (uint16_t i = 0; i < len; i++) buf[i] = ((const uint8_t *)frame)[i];
    uint16_t tx_len = len;
    if (tx_len < 60) {
        for (uint16_t i = len; i < 60; i++) buf[i] = 0;
        tx_len = 60;
    }

    // Bit 15 (TOK, "transmit OK") or bit 14 (TUN, underrun) being set in this
    // slot's status register means the chip finished with whatever was sent
    // last time and the buffer is free to reuse. On the very first send for
    // each slot the status register reads as 0 from chip reset, so we treat
    // "not currently in the middle of transmitting" the same way: bit 13
    // (OWN) is set by hardware while it still owns/needs the buffer, and
    // clear once it's done with it or before anything's been sent yet.
    uint32_t timeout = 100000;
    while (timeout--) {
        uint32_t status = inl(g_nic.io_base + REG_TXSTATUS0 + slot * 4);
        if (!(status & 0x00002000) /* OWN clear -> buffer free */) break;
    }

    // Writing to TXSTATUS with the length (bits 0-12) and default settings
    // (no early-tx threshold tricks) kicks off transmission of TXADDR[slot].
    outl(g_nic.io_base + REG_TXSTATUS0 + slot * 4, tx_len & 0x1FFF);

    return true;
}

void rtl8139_poll(rtl8139_rx_cb_t cb) {
    if (!g_nic.found || !cb) return;

    // CMD register bit 0 (BUFE, "buffer empty") tells us there's nothing
    // left to read — cheaper than comparing CAPR/CBR every call.
    while (!(inb(g_nic.io_base + REG_CMD) & 0x01)) {
        uint8_t *packet_hdr = g_nic.rx_buffer + g_nic.rx_offset;

        // RX packet header layout (written by the chip before each frame):
        //   u16 status   (bit0 = ROK, "received OK")
        //   u16 length   (includes the 4-byte CRC trailer)
        //   ...frame data (length-4 bytes of real payload, then 4 bytes CRC)
        uint16_t status = (uint16_t)(packet_hdr[0] | (packet_hdr[1] << 8));
        uint16_t plen   = (uint16_t)(packet_hdr[2] | (packet_hdr[3] << 8));

        if (!(status & 0x0001) || plen < 4 || plen > 1600) {
            // Corrupt header / desynced ring — safest recovery is to reset
            // RX rather than loop forever on garbage.
            break;
        }

        uint8_t *frame_data = packet_hdr + 4;
        uint16_t frame_len = plen - 4; // strip the trailing CRC

        cb(frame_data, frame_len);

        // Advance the ring cursor past header + data + CRC, then round up
        // to a 4-byte boundary (chip requires this alignment for CAPR).
        uint32_t consumed = 4 + plen;
        g_nic.rx_offset = (g_nic.rx_offset + consumed + 3) & ~3u;
        if (g_nic.rx_offset >= 8192) g_nic.rx_offset -= 8192;

        // CAPR is offset by -16 from our real read pointer per the chip's
        // documented quirk (it leaves a 16-byte margin for the next header).
        outw(g_nic.io_base + REG_CAPR, (uint16_t)(g_nic.rx_offset - 16));
    }
}
