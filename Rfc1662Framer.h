#ifndef RFC_1662_FRAMER_H
#define RFC_1662_FRAMER_H
// =================================================================================================
//
// Copyright (c) 2016 Hillcrest Laboratories, Inc.  All rights reserved.
// HILLCREST PROPRIETARY/CONFIDENTIAL
//
// <description>
//
// =================================================================================================

/** @file @brief
*/

// =================================================================================================
// INCLUDE FILES
// =================================================================================================
#include <stddef.h>
#include <stdint.h>

// =================================================================================================
// DEFINES and MACROS
// =================================================================================================

// =================================================================================================
// DATA TYPES
// =================================================================================================

// =================================================================================================
// CLASS DEFINITION
// =================================================================================================
/** @brief Rfc1662Framer
 */
class Rfc1662Framer {

public:
    /** @brief Some applications may require that encoding be done over multiple buffers or blocks.
     * This could be due to having only parts of the message available in a single buffer while
     * the remainder of the message is spread across other buffers or because the memory available
     * for allocation to the encode buffer is limited. To facilitate this type of of operation,
     * the block type may be indicated. Spreading the encode process across multiple blocks
     * requires that the opening and closing flags be included only on the first and last blocks
     * respectively and omitted otherwise. */
    enum BlockEncode_e {
        COMPLETE, /**< A complete block is to be encoded. */
        FIRST,    /**< Include the opening flag only */
        MIDDLE,   /**< Omit both flags */
        LAST,     /**< Include the closing flag only */
    };

    explicit Rfc1662Framer(void);
    virtual ~Rfc1662Framer(void){};

    /** @brief Encodes len bytes from the src buffer into the dest buffer by byte stuffing.
     * Flag bytes are prepended and appended to the message encoded in the dest buffer based on
     * the @b be paramter.
     * @param dest pointer to where to place the encoded bytes. The size of the dest buffer
     * must be 2 * size of src buffer + 2.
     * @param src pointer to the source bytes to encode
     * @param len the number of bytes to encode
     * @param be the type of block encoding to perform
     * @return 0 if src or dest are null, 0 if len is 0, else the number of bytes in the dest
     * buffer, including flag bytes
     */
    int encode(uint8_t* dest, const uint8_t* src, size_t len, BlockEncode_e be = COMPLETE);

    /** @brief Initialize the decoder. Any previously started decoding operations will be lost.
     * @param dest a pointer to a buffer where messages will be decoded to
     * @param len the length in bytes of the dest buffer
     */
    void decodeInit(uint8_t* dest, size_t len);

    /** @brief Decode the provided buffer of bytes into the previously provided dest buffer.
     *
     * The decoded messages are stored in the dest buffer as length/message pairs
     *
     * +------------+------------+-----------------------------+------------+----
     * | length LSB | length MSB | message (length bytes long) | length LSB | ...
     * +------------+------------+-----------------------------+------------+----
     *
     * The dest buffer must not be modified outside of the framer; otherwise, partially decoded
     * messages may be lost. The decoder may provide more than one message in the dest buffer
     * for a single call. The decoder will decode complete messages even if they are provided
     * across multiple calls.
     * @param src a pointer to the bytes to decode
     * @param len the number of bytes to decode
     * @return >0 number of messages in the dest buffer
     * @return 0 there are no messages in the dest buffer
     * @return -1 the dest buffer pointer has not been initialized
     * @return -2 the length of the dest buffer has not been initialized or was 0
     * @return -3 the dest buffer overflowed during the decode operation. If the dest buffer
     * overflows the decoder must be reinitialized.
     */
    int decode(const uint8_t* src, size_t len);

    static const uint8_t FLAG;               /**< Start/end flag */
    static const uint8_t ESC;                /**< Escape character */
    static const uint8_t XOR;                /**< Exclusive OR character */
    static const int ERR_DEST_NULL;          /**< Decoder destination buffer is null */
    static const int ERR_DEST_LEN_ZERO;      /**< Destination buffer length is 0 */
    static const int ERR_DEST_OVERFLOW;      /**< Destination buffer overflowed */
    static const unsigned int NUM_LEN_BYTES; /**< Number of bytes in the length field in the
                                              * decoder dest buffer */

protected:
private:
    typedef enum DecodeState_e {
        HUNT_,       // Hunting for a flag
        START_,      // Found a flag, starting decode
        DECODE_,     // Decoding message
        DECODE_ESC_, // Decoding an escaped character
    } DecodeState_t;

    DecodeState_t state_;
    uint8_t* dest_;
    size_t destLen_;
    uint8_t* destCursor_;   // Where to store the next byte in the dest buffer
    uint8_t* destLenStore_; // Where to store the length of the message currently being decoded
};
#endif // RFC_1662_FRAMER_H
