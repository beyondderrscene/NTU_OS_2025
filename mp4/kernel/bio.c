// Custom RAID 1 version of bio.c for MP4 Problem II

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

extern int force_read_error_pbn;
extern int force_disk_fail_id;
extern int force_write_error_pbn;

#define MIRROR_OFFSET 1000

struct {
    struct spinlock lock;
    struct buf buf[NBUF];
    struct buf head;
} bcache;

void
binit(void)
{
    struct buf *b;

    initlock(&bcache.lock, "bcache");
    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;

    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        initsleeplock(&b->lock, "buffer");
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }
}

struct buf *bget(uint dev, uint blockno)
{
    struct buf *b;
    acquire(&bcache.lock);

    for (b = bcache.head.next; b != &bcache.head; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
        if (b->refcnt == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    panic("bget: no available buffers");
}

struct buf *bread(uint dev, uint blockno)
{
    if (dev >= NDEV || dev < 0) {
        panic("bread: invalid device");
    }

    struct buf *b = bget(dev, blockno);
    int pbn0 = blockno;
    int pbn1 = pbn0 + DISK1_START_BLOCK;

    int fail_disk = force_disk_fail_id;
    int pbn0_fail = (pbn0 == force_read_error_pbn && force_read_error_pbn != -1);

    if (!b->valid) {
        if (fail_disk != 0 && !pbn0_fail) {
            // Try Disk 0
            b->blockno = pbn0;
            virtio_disk_rw(b, 0);
            b->valid = 1;
        } else if (fail_disk != 1) {
            // Fallback to Disk 1
            b->blockno = pbn1;
            virtio_disk_rw(b, 0);
            b->valid = 1;
        } else {
            // Both failed, leave b->valid = 0
            b->valid = 0;
        }
        b->blockno = pbn0;  // restore blockno
    }

    return b;
}

void bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite: buffer not locked");

    uint original_blockno = b->blockno;
    int pbn0 = original_blockno;
    int pbn1 = pbn0 + DISK1_START_BLOCK;

    int fail_disk = force_disk_fail_id;
    int pbn0_fail = (pbn0 == force_read_error_pbn && force_read_error_pbn != -1);

    printf(
        "BW_DIAG: PBN0=%d, PBN1=%d, sim_disk_fail=%d, sim_pbn0_block_fail=%d\n",
        pbn0, pbn1, fail_disk, pbn0_fail
    );

    if (fail_disk == 0) {
        printf("BW_ACTION: SKIP_PBN0 (PBN %d) due to simulated Disk 0 failure.\n", pbn0);
    } else if (pbn0_fail) {
        printf("BW_ACTION: SKIP_PBN0 (PBN %d) due to simulated PBN0 block failure.\n", pbn0);
    } else {
        printf("BW_ACTION: ATTEMPT_PBN0 (PBN %d).\n", pbn0);
        b->blockno = pbn0;
        virtio_disk_rw(b, 1);
    }

    if (fail_disk == 1) {
        printf("BW_ACTION: SKIP_PBN1 (PBN %d) due to simulated Disk 1 failure.\n", pbn1);
    } else {
        printf("BW_ACTION: ATTEMPT_PBN1 (PBN %d).\n", pbn1);
        b->blockno = pbn1;
        virtio_disk_rw(b, 1);
    }

    b->blockno = original_blockno;  // restore block number
}

void brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse: not locked");

    releasesleep(&b->lock);
    acquire(&bcache.lock);
    b->refcnt--;

    if (b->refcnt == 0) {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }

    release(&bcache.lock);
}

void bpin(struct buf *b) {
    acquire(&bcache.lock);
    b->refcnt++;
    release(&bcache.lock);
}

void bunpin(struct buf *b) {
    acquire(&bcache.lock);
    b->refcnt--;
    release(&bcache.lock);
}
