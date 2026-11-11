/**
 * @file    status.h
 * @brief   Common status code returned by every function that can fail.
 *
 * Convention (see docs/architecture/coding-conventions.md):
 *  - A function that can fail returns Status_t.
 *  - A pure computation that cannot fail returns its value directly.
 *  - Every pointer argument is checked, STATUS_ERR_NULL_ARG is returned on NULL.
 */
#ifndef STATUS_H
#define STATUS_H

typedef enum
{
    STATUS_OK = 0,           /**< Success. */
    STATUS_ERR_NULL_ARG,     /**< A required pointer argument was NULL. */
    STATUS_ERR_INVALID_ARG,  /**< An argument value is out of range or inconsistent. */
    STATUS_ERR_BUSY,         /**< Resource temporarily unavailable (for example no free CAN mailbox). */
    STATUS_ERR_FULL,         /**< A bounded container has no free slot. */
    STATUS_ERR_EMPTY,        /**< A bounded container has no element. */
    STATUS_ERR_NOT_READY,    /**< Called before the module was initialised or started. */
    STATUS_ERR_HW            /**< The hardware layer reported a failure. */
} Status_t;

#endif /* STATUS_H */
