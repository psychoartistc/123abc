#ifndef ATTRIBUTES_H_
#define ATTRIBUTES_H_

/*
 * attributes.h
 * function and struct attributes */

#define __LIMINE_REQUEST __attribute__((used, section(".limine_requests")))
#define __PACKED __attribute__((packed))
#define __NORETURN __attribute__((noreturn))
#define __UNUSED __attribute__((unused))

#endif
