/*
 * main.c
 *
 *  Created on: Jul 12, 2019
 *      Author: Intern_2
 */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <xtensa/tie/xt_hifi2.h>

#define INPUT_FILE_NAME "TestSound4.wav"
#define OUTPUT_FILE_NAME "Output.wav"
#define FILE_HEADER_SIZE 44
#define BYTES_PER_SAMPLE 2
#define DATA_BUFF_SIZE 1000
#define SAMPLE_RATE 48000
#define CHANNELS 2

#define FC 200
#define Q_VALUE 0.707		//must be between 0 and 0.707

#define PI 3.14159265358979323846


typedef struct {
	ae_f64 h;
	ae_f64 l;
} F64x2;

typedef struct {
	ae_f32x2 x[2];
	ae_f32x2 y[2];
	ae_f64 remainder[2];
} BiquadBuff;

typedef struct {
	ae_f32x2 c[5];
} BiquadCoeffs;


static inline int32_t doubleToFixed31(double x);
static inline ae_f32x2 int16ToF32x2(int16_t x, int16_t y);
static inline ae_f32x2 int32ToF32x2(int32_t x, int32_t y);
static inline F64x2 putF64ToF64x2(ae_f64 h, ae_f64 l);
static inline ae_f32x2 F64x2ToF32x2(F64x2 x);
static inline ae_f64 LeftShiftA(ae_f64 x, uint8_t shift);

static inline F64x2 Mul(ae_f32x2 x, ae_f32x2 y);
static inline F64x2 Mac(F64x2 acc, ae_f32x2 x, ae_f32x2 y);
static inline F64x2 MSub(F64x2 acc, ae_f32x2 x, ae_f32x2 y);

static inline FILE * openFile(char *fileName, _Bool mode);		//if 0 - read, if 1 - write
static inline void readHeader(uint8_t *headerBuff, FILE *inputFilePtr);
static inline void writeHeader(uint8_t *headerBuff, FILE *outputFilePtr);

static inline void initializeBiquadBuff(BiquadBuff *buff);
static inline void calculateBiquadCoeffs(BiquadCoeffs *coeffs, double Fc, double Q);

static inline ae_f32x2 biquadFilter(ae_f32x2 *data, BiquadBuff *buff, BiquadCoeffs *coeffs);
static inline void run(FILE *inputFilePtr, FILE *outputFilePtr, BiquadBuff *buff, BiquadCoeffs *coeffs);

int main()
{
	FILE *inputFilePtr = openFile(INPUT_FILE_NAME, 0);
	FILE *outputFilePtr = openFile(OUTPUT_FILE_NAME, 1);
	uint8_t headerBuff[FILE_HEADER_SIZE];

	BiquadBuff buff;
	BiquadCoeffs coeffs;

	initializeBiquadBuff(&buff);
	calculateBiquadCoeffs(&coeffs, FC, Q_VALUE);

	readHeader(headerBuff, inputFilePtr);
	writeHeader(headerBuff, outputFilePtr);
	//run(inputFilePtr, outputFilePtr, buff, coeffs);
	fclose(inputFilePtr);
	fclose(outputFilePtr);

	return 0;
}


static inline int32_t doubleToFixed31(double x)
{
	if (x >= 1)
	{
		return INT32_MAX;
	}
	else if (x < -1)
	{
		return INT32_MIN;
	}

	return (int32_t)(x * (double)(1LL << 31));
}

static inline ae_f32x2 int16ToF32x2(int16_t x, int16_t y)
{
	return AE_MOVF32X2_FROMINT32X2(AE_MOVDA32X2((int32_t)x << 16, (int32_t)y << 16));
}

static inline ae_f32x2 int32ToF32x2(int32_t x, int32_t y)
{
	return AE_MOVF32X2_FROMINT32X2(AE_MOVDA32X2(x, y));
}

static inline F64x2 putF64ToF64x2(ae_f64 h, ae_f64 l)
{
	F64x2 res;
	res.h = h;
	res.l = l;

	return res;
}

static inline ae_f32x2 F64x2ToF32x2(F64x2 x)
{
	return AE_MOVF32X2_FROMINT32X2(AE_MOVDA32X2(AE_MOVINT32_FROMF64(x.h), AE_MOVINT32_FROMF64(x.l)));
}

static inline ae_f64 LeftShiftA(ae_f64 x, uint8_t shift)
{
	return AE_SLAA64S(x, shift);
}

static inline F64x2 Mul(ae_f32x2 x, ae_f32x2 y)
{
	F64x2 res;
	res.h = AE_MULF32S_HH(x, y);
	res.l = AE_MULF32S_LL(x, y);

	return res;
}

static inline F64x2 Mac(F64x2 acc, ae_f32x2 x, ae_f32x2 y)
{
	F64x2 prod = Mul(x, y);
	acc.h = AE_ADD64S(acc.h, prod.h);
	acc.l = AE_ADD64S(acc.l, prod.l);

	return acc;
}

static inline F64x2 MSub(F64x2 acc, ae_f32x2 x, ae_f32x2 y)
{
	F64x2 prod = Mul(x, y);
	acc.h = AE_SUB64S(acc.h, prod.h);
	acc.l = AE_SUB64S(acc.l, prod.l);

	return acc;
}

static inline FILE * openFile(char *fileName, _Bool mode)		//if 0 - read, if 1 - write
{
	FILE *filePtr;

	if (mode == 0)
	{
		if ((filePtr = fopen(fileName, "rb")) == NULL)
		{
			printf("Error opening input file\n");
			system("pause");
			exit(0);
		}
	}
	else
	{
		if ((filePtr = fopen(fileName, "wb")) == NULL)
		{
			printf("Error opening output file\n");
			system("pause");
			exit(0);
		}
	}

	return filePtr;
}

static inline void readHeader(uint8_t *headerBuff, FILE *inputFilePtr)
{
	if (fread(headerBuff, FILE_HEADER_SIZE, 1, inputFilePtr) != 1)
	{
		printf("Error reading input file (header)\n");
		system("pause");
		exit(0);
	}
}

static inline void writeHeader(uint8_t *headerBuff, FILE *outputFilePtr)
{
	if (fwrite(headerBuff, FILE_HEADER_SIZE, 1, outputFilePtr) != 1)
	{
		printf("Error writing output file (header)\n");
		system("pause");
		exit(0);
	}
}

static inline void initializeBiquadBuff(BiquadBuff *buff)
{
	buff->x[0] = int32ToF32x2(0, 0);
	buff->x[1] = int32ToF32x2(0, 0);
	buff->y[0] = int32ToF32x2(0, 0);
	buff->y[1] = int32ToF32x2(0, 0);
	buff->remainder[0] = 0;
	buff->remainder[1] = 0;
}

static inline void calculateBiquadCoeffs(BiquadCoeffs *coeffs, double Fc, double Q)
{
	double cD[5];
	int32_t c[5];
	double K = tan(PI * Fc / SAMPLE_RATE);
	double norm = 1 / (1 + K / Q + K * K);

	cD[0] = K * K * norm;
	cD[1] = 2 * cD[0];
	cD[2] = cD[0];
	cD[3] = 2 * (K * K - 1) * norm;
	cD[4] = (1 - K / Q + K * K) * norm;

	c[0] = doubleToFixed31(cD[0] / 2);
	c[1] = doubleToFixed31(cD[1] / 2);
	c[2] = c[0];
	c[3] = doubleToFixed31(cD[3] / 2);
	c[4] = doubleToFixed31(cD[4] / 2);

	coeffs->c[0] = int32ToF32x2(c[0], c[0]);
	coeffs->c[1] = int32ToF32x2(c[1], c[1]);
	coeffs->c[2] = int32ToF32x2(c[2], c[2]);
	coeffs->c[3] = int32ToF32x2(c[3], c[3]);
	coeffs->c[4] = int32ToF32x2(c[4], c[4]);
}

static inline ae_f32x2 biquadFilter(ae_f32x2 *data, BiquadBuff *buff, BiquadCoeffs *coeffs)
{
	F64x2 acc;
	acc.h = buff->remainder[0];
	acc.l = buff->remainder[1];

	acc = Mac(acc, coeffs->c[0], *data);
	acc = MSub(acc, coeffs->c[3], buff->y[0]);
	acc = Mac(acc, coeffs->c[1], buff->x[0]);
	acc = MSub(acc, coeffs->c[4], buff->y[1]);
	acc = Mac(acc, coeffs->c[2], buff->x[1]);

	buff->x[1] = buff->x[0];
	buff->x[0] = *data;
	buff->y[1] = buff->y[0];

	buff->remainder[0] = AE_AND64(acc.h, 0xFFFFFFFFFFFFLL);
	buff->remainder[1] = AE_AND64(acc.l, 0xFFFFFFFFFFFFLL);

	acc.h = LeftShiftA(acc.h, 1);
	acc.l = LeftShiftA(acc.l, 1);

	buff->y[0] = F64x2ToF32x2(acc);

	return buff->y[0];
}

static inline void run(FILE *inputFilePtr, FILE *outputFilePtr, BiquadBuff *buff, BiquadCoeffs *coeffs)
{
	int16_t dataBuff[DATA_BUFF_SIZE * CHANNELS];
	size_t samplesRead;
	uint32_t i;
	ae_f32x2 dataPortion;

	while (1)
	{
		samplesRead = fread(dataBuff, BYTES_PER_SAMPLE, DATA_BUFF_SIZE * CHANNELS, inputFilePtr);

		if (!samplesRead)
		{
			break;
		}

		for (i = 0; i < samplesRead / CHANNELS; i++)
		{
			dataPortion = int16ToF32x2(dataBuff[i * CHANNELS], dataBuff[i * CHANNELS + 1]);
			dataPortion = biquadFilter(&dataPortion, buff, coeffs);

			dataBuff[i * CHANNELS] = AE_MOVAD16_3(AE_MOVF16X4_FROMF32X2(dataPortion));
			dataBuff[i * CHANNELS + 1] = AE_MOVAD16_1(AE_MOVF16X4_FROMF32X2(dataPortion));
		}

		fwrite(dataBuff, BYTES_PER_SAMPLE, samplesRead, outputFilePtr);
	}
}
