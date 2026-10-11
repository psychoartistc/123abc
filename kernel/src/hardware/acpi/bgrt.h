#ifndef BGRT_H_
#define BGRT_H_

/*
 * hardware/acpi/bgrt.h
 * boot time graphics
 * simply explained, an image that the manufacturer
 * wants us to draw while booting */

// called after bgrt table is parsed
void bgrt_show();

#endif
