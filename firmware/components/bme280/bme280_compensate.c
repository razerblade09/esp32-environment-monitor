#include "bme280_compensate.h"

static uint16_t u16_le(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int16_t s16_le(const uint8_t *p)
{
    return (int16_t)u16_le(p);
}

void bme280_parse_calib(const uint8_t block1[BME280_CALIB1_LEN],
                        const uint8_t block2[BME280_CALIB2_LEN], bme280_calib_t *calib)
{
    calib->dig_T1 = u16_le(&block1[0]);
    calib->dig_T2 = s16_le(&block1[2]);
    calib->dig_T3 = s16_le(&block1[4]);
    calib->dig_P1 = u16_le(&block1[6]);
    calib->dig_P2 = s16_le(&block1[8]);
    calib->dig_P3 = s16_le(&block1[10]);
    calib->dig_P4 = s16_le(&block1[12]);
    calib->dig_P5 = s16_le(&block1[14]);
    calib->dig_P6 = s16_le(&block1[16]);
    calib->dig_P7 = s16_le(&block1[18]);
    calib->dig_P8 = s16_le(&block1[20]);
    calib->dig_P9 = s16_le(&block1[22]);
    /* block1[24] is register 0xA0, unused */
    calib->dig_H1 = block1[25];

    calib->dig_H2 = s16_le(&block2[0]);
    calib->dig_H3 = block2[2];
    /* H4 = 0xE4[7:0] << 4 | 0xE5[3:0];  H5 = 0xE6[7:0] << 4 | 0xE5[7:4] */
    calib->dig_H4 = (int16_t)(((int8_t)block2[3] * 16) | (block2[4] & 0x0F));
    calib->dig_H5 = (int16_t)(((int8_t)block2[5] * 16) | (block2[4] >> 4));
    calib->dig_H6 = (int8_t)block2[6];
}

int32_t bme280_compensate_temperature(const bme280_calib_t *c, int32_t adc_t, int32_t *t_fine)
{
    int32_t var1 = (((adc_t >> 3) - ((int32_t)c->dig_T1 << 1)) * (int32_t)c->dig_T2) >> 11;
    int32_t d = (adc_t >> 4) - (int32_t)c->dig_T1;
    int32_t var2 = (((d * d) >> 12) * (int32_t)c->dig_T3) >> 14;

    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;
}

uint32_t bme280_compensate_pressure(const bme280_calib_t *c, int32_t adc_p, int32_t t_fine)
{
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 += (var1 * (int64_t)c->dig_P5) << 17;
    var2 += (int64_t)c->dig_P4 << 35;
    var1 = ((var1 * var1 * (int64_t)c->dig_P3) >> 8) + ((var1 * (int64_t)c->dig_P2) << 12);
    var1 = (((int64_t)1 << 47) + var1) * (int64_t)c->dig_P1 >> 33;

    if (var1 == 0) {
        return 0; /* avoid division by zero */
    }

    int64_t p = 1048576 - adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)c->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)c->dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->dig_P7 << 4);
    return (uint32_t)p;
}

uint32_t bme280_compensate_humidity(const bme280_calib_t *c, int32_t adc_h, int32_t t_fine)
{
    int32_t v = t_fine - 76800;

    v = (((((adc_h << 14) - ((int32_t)c->dig_H4 << 20) - ((int32_t)c->dig_H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)c->dig_H6) >> 10) * (((v * (int32_t)c->dig_H3) >> 11) + 32768)) >>
             10) +
            2097152) *
               (int32_t)c->dig_H2 +
           8192) >>
          14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)c->dig_H1) >> 4);
    v = (v < 0) ? 0 : v;
    v = (v > 419430400) ? 419430400 : v;
    return (uint32_t)(v >> 12);
}
