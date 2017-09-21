/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Class which handles the creation and use of Evio & HIPO Records.<p>
 *     
 * <pre>
 * RECORD STRUCTURE:
 *
 *               Uncompressed                                      Compressed
 *
 *    +----------------------------------+            +----------------------------------+
 *    |       General Record Header      |            |       General Record Header      |
 *    +----------------------------------+            +----------------------------------+
 *
 *    +----------------------------------+ ---------> +----------------------------------+
 *    |           Index Array            |            |        Compressed Data           |
 *    +----------------------------------+            |             Record               |
 *                                                    |                                  |
 *    +----------------------------------+            |                                  |
 *    |           User Header            |            |                  ----------------|
 *    |                                  |            |                  |    Pad 3      |
 *    |                  ----------------|            +----------------------------------+
 *    |                  |    Pad 1      |           ^
 *    +----------------------------------+          /
 *                                                 /
 *    +----------------------------------+       /
 *    |           Data Record            |     /
 *    |                                  |    /
 *    |                  ----------------|   /
 *    |                  |    Pad 2      | /
 *    +----------------------------------+
 *
 *
 *
 *
 * GENERAL RECORD HEADER STRUCTURE ( see RecordHeader.java )
 *
 *    +----------------------------------+
 *  1 |         Record Length            | // 32bit words, inclusive
 *    +----------------------------------+
 *  2 +         Record Number            |
 *    +----------------------------------+
 *  3 +         Header Length            | // 14 (words)
 *    +----------------------------------+
 *  4 +       Event (Index) Count        |
 *    +----------------------------------+
 *  5 +      Index Array Length          | // bytes
 *    +-----------------------+---------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     | // bytes
 *    +------+---------------------------+
 * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits)
 *    +----------------------------------+
 * 11 +        General Register 1        | // UID 1st (64 bits)
 *    +--                              --+
 * 12 +                                  |
 *    +----------------------------------+
 * 13 +        General Register 2        | // UID 2nd (64 bits)
 *    +--                              --+
 * 14 +                                  |
 *    +----------------------------------+
 * </pre>
 *
 * @version 6.0
 * @since 6.0 9/6/17
 * @author gavalian
 * @author timmer
 */
public class RecordOutputStream {
    
    /** Maximum number of events per record. */
    private static final int ONE_MEG = 1024*1024;


    /** Maximum number of events per record. */
    private int MAX_EVENT_COUNT = ONE_MEG;

    /** Size of some internal buffers in bytes. */
    private int MAX_BUFFER_SIZE = 8*ONE_MEG;

    /** Size of buffer holding built record in bytes. */
    private int RECORD_BUFFER_SIZE = 9*ONE_MEG;

    /** This buffer stores event lengths ONLY. */
    private ByteBuffer recordIndex;
    
    /** This buffer stores event data ONLY. */
    private ByteBuffer recordEvents;
    
    /** This buffer stores data that will be compressed. */
    private ByteBuffer recordData;

    /** Buffer in which to put constructed (& compressed) binary record.
     * Code is written so that it works whether or not it's backed by an array. */
    private ByteBuffer recordBinary;
    
    /** Compression information & type. */
    private Compressor dataCompressor;

    /** Header of this Record. */
    private RecordHeader header;

    /** Number of events written to this Record. */
    private int eventCount;

    /** Number of valid bytes in recordIndex buffer */
    private int indexSize;
    
    /** Number of valid bytes in recordEvents buffer. */
    private int eventSize;

    /** Byte order of record byte arrays to build. */
    private ByteOrder byteOrder;

    
    /** Default, no-arg constructor. */
    public RecordOutputStream(){
        dataCompressor = new Compressor();
        header = new RecordHeader();
        header.setCompressionType(Compressor.RECORD_COMPRESSION_LZ4);
        byteOrder = ByteOrder.LITTLE_ENDIAN;
        allocate();
    }

    /**
     * Constructor with arguments.
     * @param order byte order of built record byte arrays.
     * @param maxEventCount max number of events this record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes this record can hold.
     *                      Value of < 8MB results in default of 8MB.
     */
    public RecordOutputStream(ByteOrder order, int maxEventCount, int maxBufferSize) {
        dataCompressor = new Compressor();
        header = new RecordHeader();
        header.setCompressionType(Compressor.RECORD_COMPRESSION_LZ4);
        byteOrder = order;

        if (maxEventCount > 0) {
            MAX_EVENT_COUNT = maxEventCount;
        }

        if (maxBufferSize > MAX_BUFFER_SIZE) {
            MAX_BUFFER_SIZE = maxBufferSize;
            RECORD_BUFFER_SIZE = maxBufferSize + ONE_MEG;
        }

        allocate();
    }

    /**
     * Get the general header of this record.
     * @return general header of this record.
     */
    public RecordHeader getHeader() {return header;}

    /**
     * Get the number of events written so far into the buffer
     * @return number of events written so far into the buffer
     */
    public int getEventCount() {return eventCount;}

    /**
     * Get the internal ByteBuffer used to construct binary representation of this record.
     * @return internal ByteBuffer used to construct binary representation of this record.
     */
    public ByteBuffer getBinaryBuffer() {return recordBinary;}

    /**
     * Get the byte order of the record to be built.
     * @return byte order of the record to be built.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
     * Set the byte order of the record to be built.
     * Only use this internally to the package.
     * @param order byte order of the record to be built.
     */
    void setByteOrder(ByteOrder order) {byteOrder = order;}

    /** Allocates all buffers for constructing the record stream. */
    private void allocate(){

        recordIndex = ByteBuffer.wrap(new byte[MAX_EVENT_COUNT*4]);
        recordIndex.order(byteOrder);

        recordEvents = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
        recordEvents.order(byteOrder);

        // Making this a direct buffer slow it down by 6%
        recordData = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
        recordData.order(byteOrder);

        // Trying to compress random data will expand it, so create a cushion

        // Using a direct buffer takes 2/3 the time of an array-backed buffer
        //recordBinary = ByteBuffer.allocateDirect(RECORD_BUFFER_SIZE);
        recordBinary = ByteBuffer.wrap(new byte[RECORD_BUFFER_SIZE]);
        recordBinary.order(byteOrder);
    }
    
    /**
     * Adds an event's byte[] array into the record.
     * If a single event is too large for the internal buffers,
     * more memory is allocated. This is important for an application such as
     * CODA online in which event building may not fail due to a large event size.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     * 
     * @param event  event's ByteBuffer object
     * @return true if event was added, false if the buffer is full or
     *         event count limit exceeded
     */
    public boolean addEvent(ByteBuffer event) {

        int length = event.remaining();

        // If we receive a single event larger than our memory, we must accommodate this
        // by increasing our internal buffer size(s). Cannot simply refuse to write an
        // event during event building for example.
        if (eventCount < 1 &&
                ((4 + RecordHeader.HEADER_SIZE_BYTES + length) > MAX_BUFFER_SIZE)) {
            // Allocate roughly what we need + 1MB
            MAX_BUFFER_SIZE = length + ONE_MEG;
            RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
            allocate();
            // TODO: Don't think we want to do this
            reset();
        }

        if ((eventCount + 1 > MAX_EVENT_COUNT) ||
            ((indexSize + 4 + eventSize + RecordHeader.HEADER_SIZE_BYTES + length) >= MAX_BUFFER_SIZE)) {
            //System.out.println(" the record is FULL..... INDEX SIZE = "
            //        + indexSize + ", DATA SIZE = " + eventSize + " bytes");
            return false;
        }

        // Add event data (position of recordEvents buffer is incremented)
        if (event.hasArray()) {
            // recordEvents backing array's offset = 0
            int pos = recordEvents.position();
            System.arraycopy(event.array(),
                             event.arrayOffset() + event.position(),
                             recordEvents.array(), pos, length);
            recordEvents.position(pos + length);

            // Same as below, but above method should be a lot faster:
            // recordEvents.put(event.array(), event.arrayOffset() + event.position(), length);
        }
        else {
            recordEvents.put(event);
        }
        eventSize += length;

        // Add 1 more index
        recordIndex.putInt(indexSize, length);
        indexSize += 4;

        eventCount++;

        return true;
    }

    /**
     * Adds an event's byte[] array into the record.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     *
     * @param event    event's byte array
     * @param position offset into event byte array from which to begin reading
     * @param length   number of bytes from byte array to add
     * @return true if event was added, false if the buffer is full or
     *         event count limit exceeded
     */
    public boolean addEvent(byte[] event, int position, int length){

        if( (eventCount + 1 > MAX_EVENT_COUNT) ||
            ((indexSize + eventSize + RecordHeader.HEADER_SIZE_BYTES + length) >= MAX_BUFFER_SIZE)) {
            //System.out.println(" the record is FULL..... INDEX SIZE = "
            //        + (indexSize/4) + " DATA SIZE = " + eventSize);
            return false;
        }

        //recordEvents.position(eventSize);
        // TODO: check length if len%4 = 0 ? Error handling?

        // Add event data (position of recordEvents buffer is incremented)
        int pos = recordEvents.position();
        System.arraycopy(event, position, recordEvents.array(), pos, length);
        recordEvents.position(pos + length);
        // Same as below, but above method should be a lot faster:
        //recordEvents.put(event, position, length);
        eventSize += length;

        // Add 1 more index
        recordIndex.putInt(indexSize, length);
        indexSize += 4;

        eventCount++;

        return true;
    }

    /**
     * Adds an event's byte[] array into the record.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     * 
     * @param event event's byte array
     * @return true if event was added, false if the buffer is full.
     */
    public boolean addEvent(byte[] event){
        return addEvent(event,0, event.length);
    }

    /**
     * Reset internal buffers. The buffer is ready to receive new data.
     * Also resets the header including removing any compression.
     */
    public void reset() {
        indexSize  = 0;
        eventSize  = 0;
        eventCount = 0;

        recordData.clear();
        recordIndex.clear();
        recordEvents.clear();
        recordBinary.clear();

        // TODO: This may do way too much! Think about this more.
        header.reset();
    }

    /**
     * Reset internal buffers and set the buffer in which to build this record.
     * The given buffer should be made ready to receive new data by setting its
     * position and limit properly. It's byte order is set to the same as this writer's.
     * The argument ByteBuffer can be retrieved by calling {@link #getBinaryBuffer()}.
     * @param buf buffer in which to build record.
     * @throws HipoException if buf is too small (must be >= RECORD_BUFFER_SIZE).
     */
    public void setBuffer(ByteBuffer buf) throws HipoException {
        reset();
        recordBinary = buf;
        
        if (buf.order() != byteOrder) {
            System.out.println("setBuffer(): warning, changing byte order!");
        }

        if (buf.capacity() < RECORD_BUFFER_SIZE) {
            throw new HipoException("buffer too small, must be >= " + RECORD_BUFFER_SIZE + " bytes");
        }

        recordBinary.order(byteOrder);
    }

    /**
     * Builds the record. Compresses data, header is constructed,
     * then header & data written into internal buffer.
     */
    public void build(){

        int compressionType = header.getCompressionType();

        // Write index & event arrays

        // If compressing data ...
        if (compressionType > 0) {
            // Write into a single, temporary buffer
            recordData.position(0);
            recordData.put(  recordIndex.array(), 0, indexSize);
            recordData.put( recordEvents.array(), 0, eventSize);
        }
        // If NOT compressing data ...
        else {
            // Write directly into final buffer, past where header will go
            recordBinary.position(RecordHeader.HEADER_SIZE_BYTES);
            recordBinary.put(  recordIndex.array(), 0, indexSize);
            recordBinary.put( recordEvents.array(), 0, eventSize);
        }

        // Since hipo/evio data is padded, all data to be written is already padded
        int uncompressedDataSize = indexSize + eventSize;

        // Compress that temporary buffer into destination buffer
        // (skipping over where record header will be written).
        int compressedSize = 0;
        try {
            switch (compressionType) {
                case 1:
                    // LZ4 fastest compression
                    if (recordBinary.hasArray() && recordData.hasArray()) {
                        compressedSize = dataCompressor.compressLZ4(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(), RecordHeader.HEADER_SIZE_BYTES,
                                (recordBinary.array().length -
                                        RecordHeader.HEADER_SIZE_BYTES));
                    }
                    else {
                        compressedSize = dataCompressor.compressLZ4(
                                recordData, 0, uncompressedDataSize,
                                recordBinary, RecordHeader.HEADER_SIZE_BYTES,
                               (recordBinary.capacity() -
                                       RecordHeader.HEADER_SIZE_BYTES));
                    }
                    // Length of compressed data in bytes
                    header.setCompressedDataLength(compressedSize);
                    // Length of entire record in bytes (don't forget padding!)
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 2:
                    // LZ4 highest compression
                    if (recordBinary.hasArray() && recordData.hasArray()) {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(), RecordHeader.HEADER_SIZE_BYTES,
                                (recordBinary.array().length -
                                        RecordHeader.HEADER_SIZE_BYTES));
                    }
                    else {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData, 0, uncompressedDataSize,
                                recordBinary, RecordHeader.HEADER_SIZE_BYTES,
                               (recordBinary.capacity() -
                                       RecordHeader.HEADER_SIZE_BYTES));
                    }
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 3:
                    // GZIP compression
                    byte[] gzippedData = dataCompressor.compressGZIP(recordData.array(), 0,
                                                                     uncompressedDataSize);
                    recordBinary.position(RecordHeader.HEADER_SIZE_BYTES);
                    recordBinary.put(gzippedData);
                    compressedSize = gzippedData.length;
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 0:
                default:
                    // No compression
                    header.setCompressedDataLength(0);
                    header.setLength(uncompressedDataSize + RecordHeader.HEADER_SIZE_BYTES);
            }
        }
        catch (HipoException e) {/* should not happen */}
        //System.out.println(" DATA SIZE = " + dataBufferSize + "  COMPRESSED SIZE = " + compressedSize);

        // Set the rest of the header values
        header.setEntries(eventCount);
        header.setDataLength(eventSize);
        header.setIndexLength(indexSize);

        // Go back and write header into destination buffer
        recordBinary.position(0);
        header.writeHeader(recordBinary);

        // Make ready to read
        recordBinary.position(0).limit(eventSize);
    }

    /**
     * Builds the record. Compresses data, header is constructed,
     * then header & data written into internal buffer.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param userHeader user's ByteBuffer which must be ready to read
     */
    public void build(ByteBuffer userHeader){

        // Arg check
        if (userHeader == null) {
            build();
            return;
        }

        //int userhSize = userHeader.array().length; // May contain unused bytes at end!
        // How much user-header data do we actually have?
        int userHeaderSize = userHeader.remaining();

//        System.out.println("  INDEX = 0 " + indexSize + "  " + (indexSize + userHeaderSize)
//                + "  DIFF " + userHeaderSize);

        int compressionType = header.getCompressionType();
        int uncompressedDataSize = indexSize;

        // If compressing data ...
        if (compressionType > 0) {
            // Write into a single, temporary buffer the following:

            // 1) uncompressed index array
            recordData.position(0);
            // Note, put() will increment position
            recordData.put(recordIndex.array(), 0, indexSize);

            // 2) uncompressed user header array
            recordData.put(userHeader.array(), 0, userHeaderSize);
            // Account for unpadded user header.
            // This will find the user header length in words & account for padding.
            header.setUserHeaderLength(userHeaderSize);
            // Hop over padded user header length
            uncompressedDataSize += 4*header.getUserHeaderLengthWords();
            recordData.position(uncompressedDataSize);

            // 3) uncompressed data array (hipo/evio data is already padded)
            recordData.put(recordEvents.array(), 0, eventSize);
            uncompressedDataSize += eventSize;
        }
        // If NOT compressing data ...
        else {
            // Write directly into final buffer, past where header will go
            recordBinary.position(RecordHeader.HEADER_SIZE_BYTES);

            // 1) uncompressed index array
            recordBinary.put(recordIndex.array(), 0, indexSize);

            // 2) uncompressed user header array
            recordBinary.put(userHeader.array(), 0, userHeaderSize);
            header.setUserHeaderLength(userHeaderSize);
            uncompressedDataSize += 4*header.getUserHeaderLengthWords();
            recordBinary.position(uncompressedDataSize + RecordHeader.HEADER_SIZE_BYTES);

            // 3) uncompressed data array (hipo/evio data is already padded)
            recordBinary.put(recordEvents.array(), 0, eventSize);
            uncompressedDataSize += eventSize;
        }

        //System.out.println(" TOTAL SIZE = " + dataBufSize);

        // Compress that temporary buffer into destination buffer
        // (skipping over where record header will be written).
        int compressedSize = 0;
        try {
            switch (compressionType) {
                case 1:
                    // LZ4 fastest compression
                    if (recordBinary.hasArray() && recordData.hasArray()) {
                        compressedSize = dataCompressor.compressLZ4(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(), RecordHeader.HEADER_SIZE_BYTES,
                                (recordBinary.array().length -
                                        RecordHeader.HEADER_SIZE_BYTES));
                    }
                    else {
                        compressedSize = dataCompressor.compressLZ4(
                                recordData, 0, uncompressedDataSize,
                                recordBinary, RecordHeader.HEADER_SIZE_BYTES,
                               (recordBinary.capacity() -
                                       RecordHeader.HEADER_SIZE_BYTES));
                    }
                    // Length of compressed data in bytes
                    header.setCompressedDataLength(compressedSize);
                    // Length of entire record in bytes (don't forget padding!)
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 2:
                    // LZ4 highest compression
                    if (recordBinary.hasArray() && recordData.hasArray()) {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData.array(), 0, uncompressedDataSize,
                                recordBinary.array(), RecordHeader.HEADER_SIZE_BYTES,
                                (recordBinary.array().length -
                                        RecordHeader.HEADER_SIZE_BYTES));
                    }
                    else {
                        compressedSize = dataCompressor.compressLZ4Best(
                                recordData, 0, uncompressedDataSize,
                                recordBinary, RecordHeader.HEADER_SIZE_BYTES,
                               (recordBinary.capacity() -
                                       RecordHeader.HEADER_SIZE_BYTES));
                    }
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 3:
                    // GZIP compression
                    byte[] gzippedData = dataCompressor.compressGZIP(recordData.array(), 0,
                                                                     uncompressedDataSize);
                    recordBinary.position(RecordHeader.HEADER_SIZE_BYTES);
                    recordBinary.put(gzippedData);
                    compressedSize = gzippedData.length;
                    header.setCompressedDataLength(compressedSize);
                    header.setLength(4*header.getCompressedDataLengthWords() +
                                             RecordHeader.HEADER_SIZE_BYTES);
                    break;

                case 0:
                default:
                    // No compression
                    header.setCompressedDataLength(0);
                    header.setLength(uncompressedDataSize + RecordHeader.HEADER_SIZE_BYTES);
            }
        }
        catch (HipoException e) {/* should not happen */}
        //System.out.println(" DATA SIZE = " + dataBufferSize + "  COMPRESSED SIZE = " + compressedSize);

        // Set header values (user header length already set above)
        header.setEntries(eventCount);
        header.setDataLength(eventSize);
        header.setIndexLength(indexSize);

        // Go back and write header into destination buffer
        recordBinary.position(0);
        header.writeHeader(recordBinary);

        // Make ready to read
        recordBinary.position(0).limit(eventSize);
    }
}
