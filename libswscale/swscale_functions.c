#include <inttypes.h>


void (*rgb24tobgr32)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb24tobgr16)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb24tobgr15)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb32tobgr24)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb32to16)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb32to15)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb15to16)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb15tobgr24)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb15to32)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb16to15)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb16tobgr24)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb16to32)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb24tobgr24)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb24to16)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb24to15)(const uint8_t *src, uint8_t *dst, long src_size);
void (*shuffle_bytes_2103)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb32tobgr16)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb32tobgr15)(const uint8_t *src, uint8_t *dst, long src_size);

void (*yv12toyuy2)(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc, uint8_t *dst,
                   long width, long height,
                   long lumStride, long chromStride, long dstStride);
void (*yv12touyvy)(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc, uint8_t *dst,
                   long width, long height,
                   long lumStride, long chromStride, long dstStride);
void (*yuv422ptoyuy2)(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc, uint8_t *dst,
                      long width, long height,
                      long lumStride, long chromStride, long dstStride);
void (*yuv422ptouyvy)(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc, uint8_t *dst,
                      long width, long height,
                      long lumStride, long chromStride, long dstStride);
void (*yuy2toyv12)(const uint8_t *src, uint8_t *ydst, uint8_t *udst, uint8_t *vdst,
                   long width, long height,
                   long lumStride, long chromStride, long srcStride);
void (*rgb24toyv12)(const uint8_t *src, uint8_t *ydst, uint8_t *udst, uint8_t *vdst,
                    long width, long height,
                    long lumStride, long chromStride, long srcStride);
void (*planar2x)(const uint8_t *src, uint8_t *dst, long width, long height,
                 long srcStride, long dstStride);
void (*interleaveBytes)(const uint8_t *src1, const uint8_t *src2, uint8_t *dst,
                        long width, long height, long src1Stride,
                        long src2Stride, long dstStride);
void (*vu9_to_vu12)(const uint8_t *src1, const uint8_t *src2,
                    uint8_t *dst1, uint8_t *dst2,
                    long width, long height,
                    long srcStride1, long srcStride2,
                    long dstStride1, long dstStride2);
void (*yvu9_to_yuy2)(const uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
                     uint8_t *dst,
                     long width, long height,
                     long srcStride1, long srcStride2,
                     long srcStride3, long dstStride);
void (*uyvytoyuv420)(uint8_t *ydst, uint8_t *udst, uint8_t *vdst, const uint8_t *src,
                     long width, long height,
                     long lumStride, long chromStride, long srcStride);
void (*uyvytoyuv422)(uint8_t *ydst, uint8_t *udst, uint8_t *vdst, const uint8_t *src,
                     long width, long height,
                     long lumStride, long chromStride, long srcStride);
void (*yuyvtoyuv420)(uint8_t *ydst, uint8_t *udst, uint8_t *vdst, const uint8_t *src,
                     long width, long height,
                     long lumStride, long chromStride, long srcStride);
void (*yuyvtoyuv422)(uint8_t *ydst, uint8_t *udst, uint8_t *vdst, const uint8_t *src,
                     long width, long height,
                     long lumStride, long chromStride, long srcStride);
