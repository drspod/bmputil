#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <argp.h>

#include <magick/MagickCore.h>

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)<(b)?(b):(a))

const char* argp_program_version = "Bitmap Util 0.1";

const struct argp_option options[] = {
    {"Automatically detects the image width of raw RGB data, and writes a formatted image to a file. "
            "If the width is already known then detection can be disabled with [-w] to just write the output image file. "
            "If only width detection is required then writing of output file can be disabled [--detect-only].", 0, 0, OPTION_DOC | OPTION_NO_USAGE, "", 0},
    {"", 0, 0, OPTION_DOC | OPTION_NO_USAGE, "", 10},
    {"input-file", 0, "FILE", OPTION_DOC | OPTION_NO_USAGE, "input bitmap file path", 11},
    {"output-file", 0, "FILE", OPTION_DOC | OPTION_NO_USAGE, "output file path. File extension determines image format [default: png]", 12},
    {"Input options:", 0, 0, OPTION_DOC | OPTION_NO_USAGE, "", 20},
    {"bpp", 'b', "int", 0, "bits per pixel of the input file (valid values: 15, 16, 24) [default: 15]", 21},
    {"big-endian", 260, 0, 0, "15/16bpp values are in big-endian order [default: yes]", 22},
    {"little-endian", 261, 0, 0, "15/16bpp values are in little-endian order [default: no]", 22},
    {"offset", 'o', "int", 0, "Offset into the file of the start of RGB data [default: 0]", 23},
    {"channels", 'c', "BGR|RGB|...", 0, "Ordering of color channels [default: BGR]", 23},
    {"Output options:", 0, 0, OPTION_DOC | OPTION_NO_USAGE, "", 30},
    {"detect-only", 'n', 0, 0, "Do not write output file, just detect and print the width", 31},
    {"flip-h", 258, 0, 0, "Flip output horizontally", 31},
    {"flip-v", 259, 0, 0, "Flip output vertically", 31},
    {"Width Detection Options:", 0, 0, OPTION_DOC | OPTION_NO_USAGE, "", 40},
    {"width", 'w', "int", 0, "Specify image width (disables auto-detection) [default: auto]", 41},
    {"min-width", 256, "int", 0, "minimum image width. [default: 16]", 41},
    {"max-width", 257, "int", 0, "maximum image width. Widths up to three times this value will still be detected, but with diminishing accuracy [default: 640]", 41},
    {"", 0, 0, OPTION_DOC | OPTION_NO_USAGE, "", 50},
    {"quiet", 'q', 0, 0, "suppress all output", 51},
    {"verbose", 'v', 0, 0, "display verbose output", 51},
    {"debug", 'x', 0, 0, "write auto-correlation data to debug output file", 51},
    {"", 0, 0, OPTION_DOC | OPTION_NO_USAGE, "", 60},
    {0}
};

static char args_doc[] = "input-file [output-file]";

struct arguments
{
    char* input_file;
    char* output_file;
    char* channels;
    size_t bpp;
    size_t offset;
    size_t width;
    size_t min_width;
    size_t max_width;
    char is_little_endian;
    char flip_h;
    char flip_v;
    char detect_only;
    char quiet;
    char verbose;
    char debug;
};

static error_t parse_opt(int key, char* arg, struct argp_state* state)
{
    struct arguments* args = state->input;
    switch(key)
    {
        case 'b':
            args->bpp = atoi(arg);
            if (args->bpp != 15 && args->bpp != 16 && args->bpp != 24)
                argp_usage(state);
            break;
        case 'c':
            args->channels = arg;
            break;
        case 'o':
            args->offset= atoi(arg);
            break;
        case 'q':
            args->quiet = 1;
            break;
        case 'v':
            args->verbose = 1;
            break;
        case 'x':
            args->debug = 1;
            break;
        case 'n':
            args->detect_only = 1;
            break;
        case 'w':
            args->width = atoi(arg);
            break;
        case 256:
            args->min_width = atoi(arg);
            break;
        case 257:
            args->max_width = atoi(arg);
            break;
        case 258:
            args->flip_h = 1;
            break;
        case 259:
            args->flip_v = 1;
            break;
        case 260:
            args->is_little_endian = 0;
            break;
        case 261:
            args->is_little_endian = 1;
            break;
        case ARGP_KEY_ARG:
            if (!args->input_file)
                args->input_file = arg;
            else if (!args->output_file)
                args->output_file = arg;
            else
                argp_usage(state);
            break;
        case ARGP_KEY_END:
            if (!args->input_file)
                argp_usage(state);
            break;
        default:
            break;
    }
    return 0;
}

const struct argp argp = {options, parse_opt, args_doc};
struct arguments arguments = {NULL, NULL, "BGR", 15, 0, 0, 16, 640, 0, 0, 0, 0, 0, 0, 0};

void print(FILE* fp, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (!arguments.quiet)
        vfprintf(fp, fmt, args);
}

void print_verbose(FILE* fp, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (!arguments.quiet && arguments.verbose)
        vfprintf(fp, fmt, args);
}

typedef struct s_peak
{
    size_t idx;
    double val;
    struct s_peak* next;
} t_peak;

void add_peak(t_peak** root, size_t idx, double val)
{
    t_peak* peak = calloc(1, sizeof(t_peak));
    peak->idx = idx;
    peak->val = val;

    while (*root != NULL && (*root)->val >= peak->val)
        root = &(*root)->next;

    peak->next = *root;
    *root = peak;
}

void convert15_to_rgb(uint8_t* in_buff, size_t in_size, uint8_t** p_out_buff, size_t* p_out_size, char little_endian)
{
    *p_out_size = in_size / 2 * 3;
    *p_out_buff = (uint8_t*)malloc(*p_out_size);

    for (uint8_t* p = in_buff, *q = *p_out_buff; p - in_buff < in_size; p += 2, q += 3)
    {
        uint16_t val = (uint16_t)p[0] << 8 | (uint16_t)p[1];
        if (little_endian)
            val = (val << 8) | (val >> 8);

        q[0] = (uint8_t)((val & 0x7c00) >> 7);
        q[1] = (uint8_t)((val & 0x03e0) >> 2);
        q[2] = (uint8_t)((val & 0x001f) << 3);
    }
}

void convert16_to_rgb(uint8_t* in_buff, size_t in_size, uint8_t** p_out_buff, size_t* p_out_size, char little_endian)
{
    *p_out_size = in_size / 2 * 3;
    *p_out_buff = (uint8_t*)malloc(*p_out_size);

    for (uint8_t* p = in_buff, *q = *p_out_buff; p - in_buff < in_size; p += 2, q += 3)
    {
        uint16_t val = (uint16_t)p[1] << 8 | (uint16_t)p[0];
        if (little_endian)
            val = (val << 8) | (val >> 8);

        q[0] = (uint8_t)((val & 0xf800) >> 8);
        q[1] = (uint8_t)((val & 0x07e0) >> 3);
        q[2] = (uint8_t)((val & 0x001f) << 3);
    }
}

size_t detect_width(uint8_t* rgb_buff, size_t rgb_size, char* debug_file)
{
    size_t corr_size = min(arguments.max_width * 3 + 2, rgb_size / 3);
    double* corr_buff = (double*)calloc(corr_size, sizeof(*corr_buff));

    size_t window_size = arguments.min_width * 2 - 1;
    double* peak_window = (double*)calloc(window_size, sizeof(*peak_window));

    t_peak* peak_root = NULL;

    // auto-correlation with sliding window peak-detection
    for (size_t offset = 0; offset < corr_size; ++offset)
    {
        for (uint8_t* p = rgb_buff, *q = rgb_buff + 3 * offset;
                q - rgb_buff < rgb_size;
                p += 3, q += 3)
        {
            double p0 = p[0] - 128.0, p1 = p[1] - 128.0, p2 = p[2] - 128.0;
            double q0 = q[0] - 128.0, q1 = q[1] - 128.0, q2 = q[2] - 128.0;
            corr_buff[offset] += (p0 * q0 + p1 * q1 + p2 * q2);
        }

        if (offset > window_size / 2)
        {
            int window_idx = offset - window_size / 2 - 1;
            if (peak_window[window_idx % window_size] == corr_buff[window_idx])
            {
                add_peak(&peak_root, window_idx, corr_buff[window_idx]);
            }

            peak_window[window_idx % window_size] = 0;
        }

        for (int i = max(0, (long)offset - (long)window_size / 2);
                i <= min(corr_size - 1, offset + window_size / 2);
                ++i)
        {
            peak_window[i % window_size] = max(peak_window[i % window_size], corr_buff[offset]);
        }
    }

    // finish sliding the back half of the window, omitting the final value
    for (size_t i = corr_size - window_size / 2 - 1; i < corr_size - 1; ++i)
    {
        if (peak_window[i % window_size] == corr_buff[i])
        {
            add_peak(&peak_root, i, corr_buff[i]);
        }
    }

    // traverse peaks in descending order
    t_peak* p_peak = peak_root;
    int last = -1;
    double range_sum = 0;
    int num_ranges = 0;
    while (p_peak != NULL)
    {
        print_verbose(stderr, "Detected peak at corr_buff[%ld] = %f\n", p_peak->idx, p_peak->val);
        if (last >= 0)
        {
            // stop when encountering side-lobes
            if (p_peak->idx < last)
                break;

            range_sum += p_peak->idx - last;
            num_ranges++;
        }

        last = p_peak->idx;

        if (num_ranges >= 3)
            break;

        p_peak = p_peak->next;
    }

    // free the list
    p_peak = peak_root;
    while (p_peak != NULL)
    {
        t_peak* tmp = p_peak;
        p_peak = p_peak->next;
        free(tmp);
    }

    // output auto-correlation data points as csv
    if (debug_file)
    {
        FILE* f_out = fopen(debug_file, "w");
        if (!f_out)
        {
            print(stderr, "Failed to open debug file for writing %s: %d\n", debug_file, errno);
        }
        else
        {
            print_verbose(stderr, "Writing debug output to %s\n", debug_file);
            fprintf(f_out, "offset,\"autocorr(x, x+offset)\"\n");
            for (size_t i = 0; i < corr_size; ++i)
            {
                fprintf(f_out, "%ld,%f\n", i, corr_buff[i]);
            }

            fclose(f_out);
        }
    }

    free(peak_window);
    free(corr_buff);

    return (size_t)(range_sum / num_ranges + 0.5);
}

int write_image(uint8_t* out_buff, size_t out_size, size_t width, char* out_file, char* channels, int flip_h, int flip_v)
{
    int ret = 0;
    MagickCoreGenesis((char*) NULL, MagickFalse);

    ExceptionInfo* exc = AcquireExceptionInfo();
    GetExceptionInfo(exc);

    Image* img = ConstituteImage(width, out_size / (3 * width), channels, CharPixel, out_buff, exc);
    if (!img)
    {
        print(stderr, "Failed to constitute image: %s (%s)\n", exc->reason, out_file);
        ret = -1;
        goto tidy;
    }

    if (flip_h)
    {
        Image* tmp = img;
        img = FlopImage(img, exc);
        DestroyImage(tmp);
    }

    if (flip_v)
    {
        Image* tmp = img;
        img = FlipImage(img, exc);
        DestroyImage(tmp);
    }

    ImageInfo* info = AcquireImageInfo();
    GetImageInfo(info);

    CopyMagickString(img->filename, out_file, MaxTextExtent);
    CopyMagickString(info->filename, out_file, MaxTextExtent);

    print(stderr, "Writing output image to \"%s\"\n", out_file);

    if (MagickFalse == WriteImage(info, img))
    {
        print(stderr, "Failed to write image to %s (exception details unavailable)\n", out_file);
        ret = -1;
    }

    DestroyImageInfo(info);
    DestroyImage(img);

tidy:
    DestroyExceptionInfo(exc);
    MagickCoreTerminus();

    return ret;
}

int main(int argc, char* argv[])
{
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if (!arguments.output_file)
    {
        char* file_name = strrchr(arguments.input_file, '/');
        char* ext_part = strrchr(file_name ? file_name : arguments.input_file, '.');
        arguments.output_file = (char*)calloc(strlen(arguments.input_file) + 5, sizeof(char));
        strncat(arguments.output_file, arguments.input_file, ext_part ? ext_part - arguments.input_file : strlen(arguments.input_file));
        strcat(arguments.output_file, ".png");
    }

    print_verbose(stderr, "input_file: %s\n", arguments.input_file);
    if (!arguments.detect_only)
        print_verbose(stderr, "output_file: %s\n", arguments.output_file);

    int ret = 0;

    FILE* f_in = fopen(arguments.input_file, "r");
    if (!f_in)
    {
        print(stderr, "Failed to open %s: %d\n", arguments.input_file, errno);
        return -1;
    }

    fseek(f_in, 0, SEEK_END);
    size_t in_size = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    uint8_t* buff = (uint8_t*)malloc(in_size);

    int n = in_size;
    while (n)
        n -= fread(buff, 1, n, f_in);
    fclose(f_in);

    uint8_t* out_buff = NULL;
    size_t out_size = 0;

    if (arguments.bpp == 24)
    {
        out_buff = buff + arguments.offset;
        out_size = in_size - arguments.offset;
    }
    else if (arguments.bpp == 16)
        convert16_to_rgb(buff + arguments.offset, in_size - arguments.offset, &out_buff, &out_size, arguments.is_little_endian);
    else if (arguments.bpp == 15)
        convert15_to_rgb(buff + arguments.offset, in_size - arguments.offset, &out_buff, &out_size, arguments.is_little_endian);
    else
        return -1;

    char debug_file[4096] = {0};
    if (arguments.debug)
        snprintf(debug_file, sizeof(debug_file), "%s-autocorr.csv", arguments.output_file);

    if (!arguments.width)
    {
        arguments.width = detect_width(out_buff, out_size, arguments.debug ? debug_file : NULL);
        print_verbose(stderr, "Detected width: %ld (%s)\n", arguments.width, arguments.input_file);
        print(stdout, "%ld\n", arguments.width);
    }

    if (!arguments.detect_only)
        ret = write_image(out_buff, out_size, arguments.width, arguments.output_file,
                arguments.channels, arguments.flip_h, arguments.flip_v);

    if (arguments.bpp != 24)
        free(out_buff);
    free(buff);

    return ret;
}

