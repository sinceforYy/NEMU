#include "common.h"
#include "device/map.h"

#include "mmc.h"

#define SD_MMIO 0xa3000000

// http://www.files.e-shop.co.il/pdastore/Tech-mmc-samsung/SEC%20MMC%20SPEC%20ver09.pdf

// This is a simple hardware implementation of linux/drivers/mmc/host/bcm2835.c
// No DMA and IRQ is supported, so the driver must be modified to start PIO
// right after sending the actual read/write commands.

enum {
  SDCMD, SDARG, SDTOUT, SDCDIV,
  SDRSP0, SDRSP1, SDRSP2, SDRSP3,
  SDHSTS, __PAD0, __PAD1, __PAD2,
  SDVDD, SDEDM, SDHCFG, SDHBCT,
  SDDATA, __PAD10, __PAD11, __PAD12,
  SDHBLC
};

static FILE *fp = NULL;
static uint32_t *base = NULL;
static uint32_t blkcnt = 0;
static uint32_t addr = 0;
static bool write_cmd = 0;

static void sdcard_io_handler(uint32_t offset, int len, bool is_write) {
  int idx = offset / 4;
  switch (idx) {
    case SDCMD:
      switch (base[SDCMD] & 0x3f) {
        case MMC_GO_IDLE_STATE: break;
        case MMC_SEND_OP_COND: base[SDRSP0] = 0x80ff8000; break;
        case MMC_ALL_SEND_CID:
          base[SDRSP0] = 0x00000001;
          base[SDRSP1] = 0x00000000;
          base[SDRSP2] = 0x00000000;
          base[SDRSP3] = 0x15000000;
          break;
        case 52: // ???
          break;
        case MMC_SEND_CSD:
          base[SDRSP0] = 0x92404001;
          base[SDRSP1] = 0x124b97e3;
          base[SDRSP2] = 0x0f590200;
          base[SDRSP3] = 0x8c26012a;
          break;
        case MMC_SEND_EXT_CSD: break;
        case MMC_SLEEP_AWAKE: break;
        case MMC_APP_CMD: break;
        case MMC_SET_RELATIVE_ADDR: break;
        case MMC_SELECT_CARD: break;
        case MMC_SET_BLOCK_COUNT: blkcnt = base[SDARG] & 0xffff; break;
        case MMC_READ_MULTIPLE_BLOCK:
          // TODO
          addr = base[SDARG];
          fseek(fp, addr, SEEK_SET);
          //Log("reading from addr = 0x%x", base[SDARG]);
          write_cmd = false;
          break;
        case MMC_WRITE_MULTIPLE_BLOCK:
          // TODO
          addr = base[SDARG];
          fseek(fp, addr, SEEK_SET);
          //Log("writing to addr = 0x%x", base[SDARG]);
          write_cmd = true;
          break;
        case MMC_SEND_STATUS: base[SDRSP0] = base[SDRSP1] = base[SDRSP2] = base[SDRSP3] = 0; break;
        case MMC_STOP_TRANSMISSION: break;
        default:
          panic("unhandled command = %d", base[SDCMD] & 0x3f);
      }
      break;
    case SDARG:
      break;
    case SDRSP0:
    case SDRSP1:
    case SDRSP2:
    case SDRSP3:
    case SDHBCT:  // only for debug
    case SDHSTS:
    case SDHCFG:
      break;
    case SDHBLC: break; //Log("@@@@@@@@@@ block count = %d", base[SDHBLC]); break;// only for debug
    case SDDATA:
       // TODO
       if (!write_cmd) {
         fread(&base[SDDATA], 4, 1, fp);
       }
       addr += 4;
       break;
    case SDEDM: assert(!is_write); break;
    default:
      Log("offset = 0x%x(idx = %d), is_write = %d, data = 0x%x", offset, idx, is_write, base[idx]);
      panic("unhandle offset = %d", offset);
  }
  //Log("offset = 0x%x(idx = %d), is_write = %d, data = 0x%x", offset, idx, is_write, base[idx]);
}

void init_sdcard() {
  base = (void *)new_space(0x80);
  add_mmio_map("sdhci", SD_MMIO, (void *)base, 0x80, sdcard_io_handler);

  base[SDEDM] = (8 << 4);

  fp = fopen("/home/yzh/projectn/debian.img", "r");
  assert(fp);
}
