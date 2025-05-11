#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// 双线性插值采样（单通道）
uint8_t bilinear_sample(const uint8_t *src, int width, int height, float x, float y) {
    int x0 = (int) floorf(x), x1 = x0 + 1;
    int y0 = (int) floorf(y), y1 = y0 + 1;

    float dx = x - x0, dy = y - y0;

    x0 = CLAMP(x0, 0, width - 1);
    x1 = CLAMP(x1, 0, width - 1);
    y0 = CLAMP(y0, 0, height - 1);
    y1 = CLAMP(y1, 0, height - 1);

    uint8_t p00 = src[y0 * width + x0];
    uint8_t p01 = src[y0 * width + x1];
    uint8_t p10 = src[y1 * width + x0];
    uint8_t p11 = src[y1 * width + x1];

    float w00 = (1 - dx) * (1 - dy);
    float w01 = dx * (1 - dy);
    float w10 = (1 - dx) * dy;
    float w11 = dx * dy;

    return (uint8_t) (p00 * w00 + p01 * w01 + p10 * w10 + p11 * w11 + 0.5f);
}

// Y 通道仿射
void affine_transform_y(
    const uint8_t *src, int srcW, int srcH,
    uint8_t *dst, int dstW, int dstH,
    float a, float b, float c, float d, float tx, float ty) {
    for (int y = 0; y < dstH; y++) {
        for (int x = 0; x < dstW; x++) {
            float srcX = a * x + b * y + tx;
            float srcY = c * x + d * y + ty;
            dst[y * dstW + x] = bilinear_sample(src, srcW, srcH, srcX, srcY);
        }
    }
}

// 用于处理 V/U 分量的仿射函数（通用）
void affine_transform_plane(
    const uint8_t *src, int srcW, int srcH,
    uint8_t *dst, int dstW, int dstH,
    float a, float b, float c, float d, float tx, float ty) {
    for (int y = 0; y < dstH; y++) {
        for (int x = 0; x < dstW; x++) {
            float srcX = a * x + b * y + tx;
            float srcY = c * x + d * y + ty;
            dst[y * dstW + x] = bilinear_sample(src, srcW, srcH, srcX, srcY);
        }
    }
}

// 高质量 VU 仿射（V 和 U 分离后分别插值，再交错写入）
void affine_transform_vu_high_quality(
    const uint8_t *srcVU, int srcW, int srcH,
    uint8_t *dstVU, int dstW, int dstH,
    float a, float b, float c, float d, float tx, float ty) {
    int srcUVW = srcW;
    int srcUVH = srcH / 2;
    int dstUVW = dstW;
    int dstUVH = dstH / 2;

    int planeSize = (dstUVW / 2) * dstUVH;

    uint8_t *srcV = (uint8_t *) malloc(planeSize * 4); // 原始可能更大
    uint8_t *srcU = (uint8_t *) malloc(planeSize * 4);

    // 分离 V 和 U
    for (int y = 0; y < srcUVH; y++) {
        for (int x = 0; x < srcUVW; x += 2) {
            int idx = y * srcUVW + x;
            int p = y * (srcUVW / 2) + x / 2;
            srcV[p] = srcVU[idx]; // V
            srcU[p] = srcVU[idx + 1]; // U
        }
    }

    // 目标 V 和 U 平面
    uint8_t *dstV = (uint8_t *) malloc(planeSize);
    uint8_t *dstU = (uint8_t *) malloc(planeSize);

    // 注意：UV 分辨率是原图 1/2
    affine_transform_plane(srcV, srcUVW / 2, srcUVH, dstV, dstUVW / 2, dstUVH,
                           a / 2, b / 2, c / 2, d / 2, tx / 2, ty / 2);
    affine_transform_plane(srcU, srcUVW / 2, srcUVH, dstU, dstUVW / 2, dstUVH,
                           a / 2, b / 2, c / 2, d / 2, tx / 2, ty / 2);

    // 合并 VU
    for (int y = 0; y < dstUVH; y++) {
        for (int x = 0; x < dstUVW / 2; x++) {
            int idx = y * dstUVW + x * 2;
            int p = y * (dstUVW / 2) + x;
            dstVU[idx] = dstV[p]; // V
            dstVU[idx + 1] = dstU[p]; // U
        }
    }

    free(srcV);
    free(srcU);
    free(dstV);
    free(dstU);
}

// 构造仿射变换矩阵（含逆中心变换）
void build_affine_matrix(
    float angle_deg, float scale,
    float cx, float cy,
    float dx, float dy,
    float *a, float *b, float *c, float *d, float *tx, float *ty) {
    float angle = angle_deg * (float) (M_PI / 180.0);
    float cosA = cosf(angle) * scale;
    float sinA = sinf(angle) * scale;

    *a = cosA;
    *b = -sinA;
    *c = sinA;
    *d = cosA;

    *tx = cx - (*a * dx + *b * dy);
    *ty = cy - (*c * dx + *d * dy);
}

// 高质量仿射接口
void nv21_affine_transform(
    const uint8_t *src_nv21, int srcW, int srcH,
    uint8_t *dst_nv21, int dstW, int dstH,
    float a, float b, float c, float d, float tx, float ty) {
    const uint8_t *srcY = src_nv21;
    const uint8_t *srcVU = src_nv21 + srcW * srcH;

    uint8_t *dstY = dst_nv21;
    uint8_t *dstVU = dst_nv21 + dstW * dstH;

    affine_transform_y(srcY, srcW, srcH, dstY, dstW, dstH, a, b, c, d, tx, ty);
    affine_transform_vu_high_quality(srcVU, srcW, srcH, dstVU, dstW, dstH, a, b, c, d, tx, ty);
}

// 示例：读取 input.nv21 -> 输出 output.nv21
int main() {
    const int srcW = 640, srcH = 480;
    const int dstW = 320, dstH = 240;

    size_t src_size = srcW * srcH * 3 / 2;
    size_t dst_size = dstW * dstH * 3 / 2;

    uint8_t *src_nv21 = (uint8_t *) malloc(src_size);
    uint8_t *dst_nv21 = (uint8_t *) malloc(dst_size);

    FILE *fsrc = fopen("../input_640x480.nv21", "rb");
    if (!fsrc) {
        perror("打开输入失败");
        return 1;
    }
    fread(src_nv21, 1, src_size, fsrc);
    fclose(fsrc);

    float a, b, c, d, tx, ty;
    build_affine_matrix(0.0f, 1.0f, srcW / 2.0f, srcH / 2.0f, dstW / 2.0f, dstH / 2.0f, &a, &b, &c, &d, &tx, &ty);

    // a = 0.4330127018922194f;
    // b = 0.25f;
    // c = 121.4359353944898f;
    // d = -0.25f;
    // tx = 0.4330127018922194f;
    // ty = 216.0769515458673f;

    nv21_affine_transform(src_nv21, srcW, srcH, dst_nv21, dstW, dstH, a, b, c, d, tx, ty);

    FILE *fdst = fopen("../output_320x240_nocv.nv21", "wb");
    fwrite(dst_nv21, 1, dst_size, fdst);
    fclose(fdst);

    free(src_nv21);
    free(dst_nv21);

    printf("✅ 成功输出: output_320x240_nocv.nv21\n");
    return 0;
}

// ffplay -f rawvideo -pixel_format nv21 -video_size 320x240 output_320x240_nocv.nv21
// ffmpeg -f rawvideo -pixel_format nv21 -video_size 320x240 -i output_320x240_nocv.nv21 -frames:v 1 output_320x240_nocv.png
