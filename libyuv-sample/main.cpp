#include "libyuv.h"
#include <iostream>
#include <png.h>
#include <jpeglib.h>

int pngTorgb(const char* pngFile, const char* jpgFile) {
    FILE *fp = fopen(pngFile, "rb");
    if (!fp) {
        std::cerr << "Error opening PNG file: " << pngFile << "\n";
        return -1;
    }
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return 1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return 1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return 1;
    }

    png_init_io(png_ptr, fp);

    // Read PNG header and info
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    std::cout << "Image width: " << width << "\n";
    std::cout << "Image height: " << height << "\n";
    std::cout << "Color type: " << (int)color_type << "\n";
    std::cout << "Bit depth: " << (int)bit_depth << "\n";

    // Set up transformations if needed (e.g., to 8-bit RGBA)
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER); // Add alpha channel if missing

    png_read_update_info(png_ptr, info_ptr); // Update info after transformations

    // Allocate memory for image data
    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return 1;
    }

    for (png_uint_32 y = 0; y < height; y++) {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));
        if (!row_pointers[y]) {
            // Handle allocation error and free previously allocated rows
            for (png_uint_32 i = 0; i < y; ++i) {
                free(row_pointers[i]);
            }
            free(row_pointers);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return 1;
        }
    }

    // Read the entire image
    png_read_image(png_ptr, row_pointers);

    // Image data is now in row_pointers. You can process it here.
    // For example, print the first pixel's RGBA values:
    if (height > 0 && width > 0) {
        printf("First pixel (RGBA): %d %d %d %d\n",
               row_pointers[0][0], row_pointers[0][1],
               row_pointers[0][2], row_pointers[0][3]);
    }

    // save JPEG file
    FILE *jpeg_fp = fopen(jpgFile, "wb");
    if (!jpeg_fp) {
        std::cerr << "Error opening JPEG file: " << jpgFile << "\n";
        return -1;
    }

    // Initialize JPEG compression
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, jpeg_fp);

    // Set JPEG parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 4; // RGBA
    cinfo.in_color_space = JCS_EXT_RGBA;
    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    // Write image data
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer[1];
        row_pointer[0] = (JSAMPROW)row_pointers[cinfo.next_scanline];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    // Clean up
    jpeg_finish_compress(&cinfo);
    fclose(jpeg_fp);

    // Clean up
    for (png_uint_32 y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    return 0;

}

int main(int, char **)
{
    std::cout << "Hello, from yuvsimpleapp!\n";
    std::cout << "LibYUV version: " << LIBYUV_VERSION << "\n";
    int  result = -1;
    const char* png_file = "../lena.png";
    const char* jpg_file = "../lena.jpg";
    // Convert PNG to RGB (JPEG) format
    result = pngTorgb(png_file, jpg_file);
    if (result != 0) {
        std::cerr << "Error converting PNG to RGB: " << result << "\n";
        return result;
    }



    return 0;

    // Example usage of a LibYUV function
    int width = 640;
    int height = 480;

    int y_stride = width;
    int u_stride = width / 2;
    int v_stride = width / 2;

    int y_size = y_stride * height;
    int u_size = u_stride * height / 2;
    int v_size = v_stride * height / 2;

    int yuv_size = y_size + u_size + v_size;
    int rgb_size = width * height * 3;

    uint8_t *src_nv21 = new uint8_t[yuv_size];
    uint8_t *i420 = new uint8_t[yuv_size];
    uint8_t *dst_rgb = new uint8_t[rgb_size];

    size_t bytes_read = 0;
    FILE *input_file = nullptr;
    FILE *output_file = nullptr;

   const char *jpeg_file = "../trumps_640x480.jpg";
   const char* nv21_file = "../output_trumps_640x480_nv21.bin";
   const char* rgb_file =  "../output_trumps_640x480_rgb24.bin";
    uint8_t *jpeg_buffer = nullptr;
    size_t jpeg_size = 0;
    input_file = fopen(jpeg_file, "rb");
    if (!input_file)
    {        std::cerr << "Error opening input JPEG file: " << jpeg_file << "\n";
        goto clean_up;
    }
    // Read the JPEG file into a buffer
    fseek(input_file, 0, SEEK_END);
    jpeg_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    jpeg_buffer = new uint8_t[jpeg_size];
    bytes_read = fread(jpeg_buffer, 1, jpeg_size, input_file);
    fclose(input_file);
    if (bytes_read != jpeg_size)
    {
        std::cerr << "Error reading JPEG data from file. Expected "
                  << jpeg_size << " bytes, but read " << bytes_read << " bytes.\n";
        goto clean_up;                  
    }

    // Convert JPEG to NV21 format
    result = libyuv::MJPGToNV21(jpeg_buffer, jpeg_size, src_nv21, y_stride, src_nv21 + y_size, u_stride + v_stride, width, height,width, height);
    delete[] jpeg_buffer;
    if (result != 0)
    {   
        std::cerr << "Error converting JPEG to NV21: " << result << "\n";
        goto clean_up;
    }

    // Save NV21 data to a file (optional)
    output_file = fopen(nv21_file, "wb");
    if (!output_file)
    {       std::cerr << "Error opening output NV21 file.\n";
        goto clean_up;
    }   
    fwrite(src_nv21, 1, yuv_size, output_file);
    fclose(output_file);    
    std::cout << "Successfully converted JPEG to NV21 and saved to " << nv21_file << ".\n";


    // char *nv21_file = "../trumps_640x480.nv21";
    // input_file = fopen(nv21_file, "rb");
    // if (!input_file)
    // {
    //     std::cerr << "Error opening input NV21 file: " << nv21_file << "\n";
    //     goto clean_up;
    // }

    // bytes_read = fread(src_nv21, 1, yuv_size, input_file);
    // fclose(input_file);
    // if (bytes_read != yuv_size)
    // {
    //     std::cerr << "Error reading NV21 data from file. Expected "
    //               << yuv_size << " bytes, but read " << bytes_read
    //               << " bytes.\n";
    //     goto clean_up;
    // }

    result = libyuv::NV21ToI420(src_nv21, y_stride, src_nv21 + y_size, u_stride + v_stride,
                                i420, y_stride, i420 + y_size, u_stride, i420 + y_size + u_size, v_stride, width, height);

    // test ok
    // result = libyuv::ConvertFromI420(i420, y_stride, i420 + y_size, u_stride, i420 + y_size + u_size, v_stride, dst_rgb, width * 3, width, height, libyuv::FOURCC_RGB3);

    result = libyuv::NV21ToRAW(src_nv21, y_stride, src_nv21 + y_size, u_stride + v_stride, dst_rgb, width * 3, width, height);


    // save rgb data to a file or process it as needed
    if (result != 0)
    {
        std::cerr << "Error converting NV21 to RGB24: " << result << "\n";
        goto clean_up;
    }

    output_file = fopen(rgb_file, "wb");
    if (!output_file)
    {
        std::cerr << "Error opening output file.\n";
        goto clean_up;
    }
    fwrite(dst_rgb, 1, rgb_size, output_file);
    fclose(output_file);

    std::cout << "Successfully converted NV21 to RGB24.\n";

clean_up:
    // Clean up
    delete[] src_nv21;
    delete[] i420;
    delete[] dst_rgb;

    std::cout << "LibYUV sample application completed successfully.\n";
    std::cout << "You can now use LibYUV functions as needed.\n";
    std::cout << "For more information, refer to the LibYUV documentation.\n";

    return 0;
}
