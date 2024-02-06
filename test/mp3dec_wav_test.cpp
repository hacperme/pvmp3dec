

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <string.h>

#include "pvmp3decoder_api.h"
#include "mp3reader.h"

using namespace std;

enum
{
    kInputBufferSize = 10 * 1024,
    kOutputBufferSize = 4608 * 2,
};

// WAV文件头结构体
typedef struct
{
    char chunkID[4];        // "RIFF"
    unsigned int chunkSize; // 文件总大小 - 8
    char format[4];         // "WAVE"
} RIFFHeader;

typedef struct
{
    char subChunkID[4];           // "fmt "
    unsigned int subChunkSize;    // fmt块大小 : 16
    unsigned short audioFormat;   // 音频格式, PCM = 1
    unsigned short numChannels;   // 声道数
    unsigned int sampleRate;      // 采样率
    unsigned int byteRate;        // 每秒数据字节数
    unsigned short blockAlign;    // 数据块对齐单位(每个采样需要的字节数)
    unsigned short bitsPerSample; // 采样位数
} FMTHeader;

typedef struct
{
    char subChunkID[4];        // "data"
    unsigned int subChunkSize; // PCM数据大小
} DataHeader;

// 写WAV头
void writeWAVHeader(FILE *fout, int sampleRate, int bitsPerSample, int numChannels, int dataSize)
{
    // RIFF头
    RIFFHeader rh;
    memcpy(rh.chunkID, "RIFF", 4);
    rh.chunkSize = 36 + dataSize;
    memcpy(rh.format, "WAVE", 4);

    // fmt块
    FMTHeader fh;
    memcpy(fh.subChunkID, "fmt ", 4);
    fh.subChunkSize = 16; // PCM fmt块大小
    fh.audioFormat = 1;   // PCM
    fh.numChannels = numChannels;
    fh.sampleRate = sampleRate;
    fh.byteRate = sampleRate * numChannels * bitsPerSample / 8;
    fh.blockAlign = numChannels * bitsPerSample / 8;
    fh.bitsPerSample = bitsPerSample;

    // data块
    DataHeader dh;
    memcpy(dh.subChunkID, "data", 4);
    dh.subChunkSize = dataSize;

    // 开始写文件
    fwrite(&rh, sizeof(rh), 1, fout);
    fwrite(&fh, sizeof(fh), 1, fout);
    fwrite(&dh, sizeof(dh), 1, fout);
}

// 主函数
int main(int argc, const char **argv)
{

    if (argc != 3)
    {
        fprintf(stderr, "Usage %s <input file> <output file>\n", argv[0]);
        return -1;
    }

    // Initialize the config.
    tPVMP3DecoderExternal config;
    config.equalizerType = flat;
    config.crcEnabled = false;

    // Allocate the decoder memory.
    uint32_t memRequirements = pvmp3_decoderMemRequirements();
    void *decoderBuf = malloc(memRequirements);
    assert(decoderBuf != NULL);

    // Initialize the decoder.
    pvmp3_InitDecoder(&config, decoderBuf);

    // Open the input file.
    Mp3Reader mp3Reader;
    bool success = mp3Reader.init(argv[1]);
    if (!success)
    {
        fprintf(stderr, "Encountered error reading %s\n", argv[1]);
        free(decoderBuf);
        return -1;
    }

    const char *outputFileName = argv[2]; // 输出WAV文件名

    // 这里需要根据实际PCM数据修改
    int sampleRate = mp3Reader.getSampleRate();   // 采样率
    int bitsPerSample = 16;                       // 位深
    int numChannels = mp3Reader.getNumChannels(); // 声道数

    // 打开输入输出文件
    FILE *fout = fopen(outputFileName, "wb");
    if (!fout)
    {
        perror("file open error");
        mp3Reader.close();
        free(decoderBuf);
        return -1;
    }

    // Allocate input buffer.
    uint8_t *inputBuf = static_cast<uint8_t *>(malloc(kInputBufferSize));
    assert(inputBuf != NULL);

    // Allocate output buffer.
    int16_t *outputBuf = static_cast<int16_t *>(malloc(kOutputBufferSize));
    assert(outputBuf != NULL);

    // Decode loop.
    int retVal = 0;
    uint32_t dataSize = 0;

    while (1)
    {
        // Read input from the file.
        uint32_t bytesRead;
        bool success = mp3Reader.getFrame(inputBuf, &bytesRead);
        if (!success)
            break;

        // Set the input config.
        config.inputBufferCurrentLength = bytesRead;
        config.inputBufferMaxLength = 0;
        config.inputBufferUsedLength = 0;
        config.pInputBuffer = inputBuf;
        config.pOutputBuffer = outputBuf;
        config.outputFrameSize = kOutputBufferSize / sizeof(int16_t);

        ERROR_CODE decoderErr;
        decoderErr = pvmp3_framedecoder(&config, decoderBuf);
        if (decoderErr != NO_DECODING_ERROR)
        {
            fprintf(stderr, "Decoder encountered error\n");
            retVal = -1;
            break;
        }
        dataSize += fwrite(outputBuf, sizeof(unsigned char), config.outputFrameSize * sizeof(int16_t), fout);
    }

    // Close input reader and output writer.
    mp3Reader.close();

    // Free allocated memory.
    free(inputBuf);
    free(outputBuf);
    free(decoderBuf);

    fseek(fout, 0, SEEK_SET);

    // 写入WAV头
    writeWAVHeader(fout, sampleRate, bitsPerSample, numChannels, dataSize);

    // 关闭文件
    fclose(fout);

    printf("mp3 decode sussess\n");
    return retVal;
}

#ifdef __cplusplus
} /*"C" */
#endif
