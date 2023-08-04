/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __MYCOMPAT_H_
#define __MYCOMPAT_H_

uint8_t SendDataToCm33SDCardBlockingMode(uint8_t *buf, uint32_t len);

uint8_t GetDataFromCm33SDCardBlockingMode(uint8_t *buf, uint32_t len);

uint8_t seekCM33SDFileBlockingMode(uint32_t pos);

uint32_t tellCM33SDFileSizeBlockingMode(void);

uint8_t closeCM33SDFileBlockingMode(void);


#endif
