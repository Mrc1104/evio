//
// Created by Carl Timmer on 2019-05-13.
//

#include "WriterMT.h"



/**
 * Default constructor. Compression is single-threaded, LZ4. Little endian.
 * <b>No</b> file is opened. Any file will have little endian byte order.
 * 1M max event count and 8M max buffer size.
 */
WriterMT::WriterMT() :
    WriterMT(HeaderType::EVIO_FILE, ByteOrder::ENDIAN_LITTLE, 0, 0,
             Compressor::LZ4, 1, true, "", nullptr, 0, 16) {
}


/**
 * Constructor with byte order. <b>No</b> file is opened.
 * File header type is evio file ({@link HeaderType#EVIO_FILE}).
 * Any dictionary will be placed in the user header which will create a conflict if
 * user tries to call {@link #open(String, byte[])} with another user header array.
 *
 * @param order byte order of written file.
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 */
WriterMT::WriterMT(const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                   Compressor::CompressionType compType, uint32_t compressionThreads) :
    WriterMT(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize,
             compType, compressionThreads, true, "", nullptr, 0, 16) {
}


/**
 * Constructor with byte order. The given file is opened so any subsequent call to open will fail.
 * This method places the dictionary and first event into the file header's user header.
 *
 * @param hType         the type of the file. If set to {@link HeaderType#HIPO_FILE},
 *                      the header will be written with the first 4 bytes set to HIPO.
 * @param order         byte order of written file.
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 * @param compType      type of data compression to do (one, lz4 fast, lz4 best, gzip)
 * @param compressionThreads number of threads doing compression simultaneously
 * @param addTrailerIndex if true, we add a record index to the trailer.
 * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
 * @param firstEvent    byte array containing an evio event to be included in userHeader.
 *                      It must be in the same byte order as the order argument.
 * @param firstEventLen number of valid bytes in firstEvent.
 * @param ringSize      number of records in supply ring, must be multiple of 2
 *                      and >= compressionThreads.
 */
WriterMT::WriterMT(const HeaderType & hType, const ByteOrder & order,
                   uint32_t maxEventCount, uint32_t maxBufferSize,
                   Compressor::CompressionType compType, uint32_t compressionThreads,
                   bool addTrailerIndex,
                   const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen,
                   uint32_t ringSize) {

    byteOrder = order;
    this->dictionary = dictionary;
    this->firstEvent = firstEvent;
    if (firstEvent == nullptr) {
        firstEventLen = 0;
    }
    firstEventLength = firstEventLen;
    this->maxEventCount = maxEventCount;
    this->maxBufferSize = maxBufferSize;
    this->addTrailerIndex = addTrailerIndex;

    compressionType = compType;
    compressionThreadCount = compressionThreads;

    recordLengths.reserve(1500);
    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);

    if (hType == HeaderType::HIPO_FILE) {
        fileHeader = FileHeader(false);
    } else {
        fileHeader = FileHeader(true);
    }

    haveDictionary = dictionary.length() > 0;
    haveFirstEvent = (firstEvent != nullptr && firstEventLen > 0);

    if (haveDictionary || haveFirstEvent)  {
        dictionaryFirstEventBuffer = createDictionaryRecord();
    }
    else {
        // Tell open() there is no dictionary/first event data
        dictionaryFirstEventBuffer.limit(0);
    }

    // Number of ring items must be >= compressionThreads
    int32_t finalRingSize = ringSize;
    if (finalRingSize < compressionThreads) {
        finalRingSize = compressionThreads;
    }
    // AND is must be multiple of 2
    finalRingSize = Disruptor::Util::ceilingNextPowerOfTwo(finalRingSize);

    if (finalRingSize != ringSize) {
        cout << "WriterMT: started w/ ring size = " << ringSize <<
        ", change to " << finalRingSize << endl;
    }

    supply = std::make_shared<RecordSupply>(finalRingSize, byteOrder,
                                            compressionThreads,
                                            maxEventCount, maxBufferSize,
                                            compressionType);

    // Get a single blank record to start writing into
    ringItem = supply->get();
    outputRecord = ringItem->getRecord();

    // TODO: start up threads ?????
}


/**
 * Constructor with filename.
 * The output file will be created with no user header.
 * File byte order is little endian.
 * @param filename output file name
 */
WriterMT::WriterMT(string & filename) : WriterMT() {
    open(filename);
}


/**
 * Constructor with filename & byte order.
 * The output file will be created with no user header.
 * Ring size is 16.
 *
 * @param filename      output file name
 * @param order         byte order of written file or null for default (little endian)
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
 * @param compressionThreads number of threads doing compression simultaneously
 * @param ringSize      number of records in supply ring, must be multiple of 2
 *                      and >= compressionThreads.
 */
WriterMT::WriterMT(string & filename, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                   Compressor::CompressionType compType, uint32_t compressionThreads) :
         WriterMT(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize,
                  compType, compressionThreads, true, "", nullptr, 0, 4) {

    open(filename);
}


//////////////////////////////////////////////////////////////////////

///**
// * Get the buffer being written to.
// * @return buffer being written to.
// */
//ByteBuffer & WriterMT::getBuffer() {return buffer;}

/**
 * Get the file's byte order.
 * @return file's byte order.
 */
const ByteOrder & WriterMT::getByteOrder() const {return byteOrder;}

/**
 * Get the file header.
 * @return file header.
 */
FileHeader & WriterMT::getFileHeader() {return fileHeader;}

///**
// * Get the internal record's header.
// * @return internal record's header.
// */
//RecordHeader & WriterMT::getRecordHeader() {return outputRecord->getHeader();}
//
///**
// * Get the internal record used to add events to file.
// * @return internal record used to add events to file.
// */
//RecordOutput & WriterMT::getRecord() {return outputRecord;}

/**
 * Convenience method that gets compression type for the file being written.
 * @return compression type for the file being written.
 */
Compressor::CompressionType WriterMT::getCompressionType() {return compressionType;}


/**
 * Does this writer add a trailer to the end of the file/buffer?
 * @return true if this writer adds a trailer to the end of the file/buffer, else false.
 */
bool WriterMT::addTrailer() const {return addingTrailer;}

/**
 * Set whether this writer adds a trailer to the end of the file/buffer.
 * @param add if true, at the end of file/buffer, add an ending header (trailer)
 *            with no index of records and no following data.
 *            Update the file header to contain a file offset to the trailer.
 */
void WriterMT::addTrailer(bool add) {addingTrailer = add;}

/**
 * Does this writer add a trailer with a record index to the end of the file?
 * Or, if writing to a buffer, is a trailer added with no index?
 * @return if writing to a file: true if this writer adds a trailer with a record index
 *         to the end of the file, else false. If writing to a buffer, true if this
 *         writer adds a traile to the end of the buffer, else false.
 */
bool WriterMT::addTrailerWithIndex() {return addTrailerIndex;}

/**
 * Set whether this writer adds a trailer with a record index to the end of the file.
 * Or, if writing to a buffer, set whether a trailer is added with no index.
 * @param addTrailingIndex if true, at the end of file, add an ending header (trailer)
 *                         with an index of all records but with no following data.
 *                         Update the file header to contain a file offset to the trailer.
 *                         If true, and writing to a buffer, add a trailer with no index
 *                         to the end of the buffer.
 */
void WriterMT::addTrailerWithIndex(bool addTrailingIndex) {
    addTrailerIndex = addTrailingIndex;
    if (addTrailingIndex) {
        addingTrailer = true;
    }
}


/**
 * Open a new file and write file header with no user header.
 * @param filename output file name
 * @throws HipoException if open already called without being followed by calling close.
 * @throws IOException if file cannot be found or IO error writing to file
 */
void WriterMT::open(string & filename) {
    open(filename, nullptr, 0);
}


/**
 * Open a file and write file header with given user header.
 * User header is automatically padded when written.
 * @param filename   name of file to write to.
 * @param userHdr    byte array representing the optional user's header.
 *                   If this is null AND dictionary and/or first event are given,
 *                   the dictionary and/or first event will be placed in its
 *                   own record and written as the user header.
 * @param userLen   length of userHdr in bytes.
 * @throws HipoException if filename arg is bad,
 *                       or if open() was already called without being followed by reset().
 * @throws IOException   if file cannot be found or IO error writing to file
 */
void WriterMT::open(string & filename, uint8_t* userHdr, uint32_t userLen) {

    if (opened) {
        throw HipoException("currently open, call reset() first");
    }

    if (filename.length() < 1) {
        throw HipoException("bad filename");
    }

    ByteBuffer fileHeaderBuffer(0);
    haveUserHeader = false;

    // User header given as arg has precedent
    if (userHdr != nullptr) {
        haveUserHeader = true;
cout << "writerMT::open: given a valid user header to write" << endl;
        fileHeaderBuffer = createHeader(userHdr, userLen);
    }
    else {
        // If dictionary & firstEvent not defined and user header not given ...
        if (dictionaryFirstEventBuffer.remaining() < 1) {
cout << "writerMT::open: given a null user header to write, userLen = " << userLen <<  endl;
            fileHeaderBuffer = createHeader(nullptr, 0);
        }
        // else place dictionary and/or firstEvent into
        // record which becomes user header
        else {
cout << "writerMT::open: given a valid dict/first ev header to write" << endl;
            fileHeaderBuffer = createHeader(dictionaryFirstEventBuffer);
        }
    }

    // Write this to file
    fileName = filename;
    // TODO: what flags??? instead of "rw"
    outFile.open(filename, ios::binary);
    outFile.write(reinterpret_cast<const char*>(fileHeaderBuffer.array()), fileHeaderBuffer.remaining());
    if (outFile.fail()) {
        throw HipoException("error opening file " + filename);
    }

    writerBytesWritten = (size_t) (fileHeader.getLength());

    // Create compression threads
    recordCompressorThreads.reserve(compressionThreadCount);
    for (int i=0; i < compressionThreadCount; i++) {
        recordCompressorThreads.emplace_back(i, compressionType, supply);
    }

    // Start compression threads
    for (int i=0; i < compressionThreadCount; i++) {
        recordCompressorThreads[i].startThread();
    }

    // Create & start writing thread
    recordWriterThreads.emplace_back(this,supply);
    recordWriterThreads[0].startThread();

    opened = true;
}


/**
 * Create a buffer representation of a record
 * containing the dictionary and/or the first event.
 * No compression.
 * @return buffer representation of record containing dictionary and/or first event,
 *         of zero size if first event and dictionary don't exist.
 */
ByteBuffer WriterMT::createDictionaryRecord() {
    return Writer::createRecord(dictionary, firstEvent, firstEventLength,
                                byteOrder, &fileHeader, nullptr);
}


/**
 * Create and return a buffer containing a general file header
 * followed by the user header given in the argument.
 * If user header is not padded to 4-byte boundary, it's done here.
 *
 * @param userHdr byte array containing a user-defined header, may be null.
 * @param userLen array length in bytes.
 * @return ByteBuffer containing a file header followed by the user-defined header
 */
ByteBuffer WriterMT::createHeader(uint8_t* userHdr, uint32_t userLen) {

cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

    // Amount of user data in bytes
    int userHeaderBytes = 0;
    if (userHdr != nullptr) {
        userHeaderBytes = userLen;
    }
    fileHeader.reset();
    if (haveUserHeader) {
        fileHeader.setBitInfo(false, false, addTrailerIndex);
    }
    else {
        fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
    }
    fileHeader.setUserHeaderLength(userHeaderBytes);

cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
    uint32_t totalLen = fileHeader.getLength();
    ByteBuffer buf(totalLen);
    buf.order(byteOrder);

    try {
cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
        fileHeader.writeHeader(buf, 0);
    }
    catch (HipoException & e) {/* never happen */}

    if (userHeaderBytes > 0) {
        std::memcpy((void *)(buf.array() + FileHeader::HEADER_SIZE_BYTES),
                    (const void *)(userHdr), userHeaderBytes);
    }

    // Get ready to read, buffer position is still 0
    buf.limit(totalLen);
    return std::move(buf);
}


/**
 * Return a buffer with a general file header followed by the given user header (userHdr).
 * The buffer is cleared and set to desired byte order prior to writing.
 * If user header is not padded to 4-byte boundary, it's done here.
 *
 * @param userHdr buffer containing a user-defined header which must be READY-TO-READ!
 * @return buffer containing a file header followed by the user-defined header.
 * @throws HipoException if writing to buffer, not file.
 */
ByteBuffer WriterMT::createHeader(ByteBuffer & userHdr) {

    int userHeaderBytes = userHdr.remaining();
    fileHeader.reset();
    cout << "WriterMT::createHeader: haveFirstEv = " << haveFirstEvent << ", have Dict = " << haveDictionary <<
    ", add Trailer w/ index = " << addTrailerIndex << endl;
    if (haveUserHeader) {
        fileHeader.setBitInfo(false, false, addTrailerIndex);
    }
    else {
        fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
    }
    fileHeader.setUserHeaderLength(userHeaderBytes);

    uint32_t totalLen = fileHeader.getLength();
    ByteBuffer buf(totalLen);
    buf.order(byteOrder);

    try {
        fileHeader.writeHeader(buf, 0);
    }
    catch (HipoException & e) {/* never happen */}

    if (userHeaderBytes > 0) {
        std::memcpy((void *)(buf.array() + FileHeader::HEADER_SIZE_BYTES),
                    (const void *)(userHdr.array() + userHdr.arrayOffset()+ userHdr.position()),
                    userHeaderBytes);
    }

    // Get ready to read, buffer position is still 0
    buf.limit(totalLen);
    return std::move(buf);
}


/**
 * Turn int into byte array.
 *
 * @param data        int to convert.
 * @param byteOrder   byte order of returned bytes.
 * @param dest        array in which to store returned bytes.
 * @param off         offset into dest array where returned bytes are placed.
 * @param destMaxSize max size in bytes of dest array.
 * @throws HipoException if dest is null or too small.
 */
void WriterMT::toBytes(uint32_t data, const ByteOrder & byteOrder,
                       uint8_t* dest, uint32_t off, uint32_t destMaxSize) {

    if (dest == nullptr || destMaxSize < 4+off) {
        throw HipoException("bad arg(s)");
    }

    if (byteOrder == ByteOrder::ENDIAN_BIG) {
        dest[off  ] = (uint8_t)(data >> 24);
        dest[off+1] = (uint8_t)(data >> 16);
        dest[off+2] = (uint8_t)(data >>  8);
        dest[off+3] = (uint8_t)(data      );
    }
    else {
        dest[off  ] = (uint8_t)(data      );
        dest[off+1] = (uint8_t)(data >>  8);
        dest[off+2] = (uint8_t)(data >> 16);
        dest[off+3] = (uint8_t)(data >> 24);
    }
}


/**
 * Write a general header as the last "header" or trailer in the file
 * optionally followed by an index of all record lengths.
 *
 * It's best <b>NOT</b> to call this directly. The way to write a trailer to
 * fis to call {@link #addTrailer(bool)} or {@link #addTrailerWithIndex(bool)}.
 * Then when {@link #close()} is called, the trailer will be written.
 *
 * @param writeIndex if true, write an index of all record lengths in trailer.
 * @throws HipoException if error writing to file.
 */
void WriterMT::writeTrailer(bool writeIndex) {

    // Keep track of where we are right now which is just before trailer
    uint64_t trailerPosition = writerBytesWritten;

    // If we're NOT adding a record index, just write trailer
    if (!writeIndex) {
        RecordHeader::writeTrailer(&headerArray[0], RecordHeader::HEADER_SIZE_BYTES, 0,
                                   recordNumber, byteOrder, nullptr, 0);

        writerBytesWritten += RecordHeader::HEADER_SIZE_BYTES;
        outFile.write(reinterpret_cast<const char *>(&headerArray[0]), RecordHeader::HEADER_SIZE_BYTES);
        if (outFile.fail()) {
            throw HipoException("error writing file " + fileName);
        }
    }
    else {
        // Create the index of record lengths & entries in proper byte order
        size_t recordLengthsBytes = 4 * recordLengths.size();
        auto recordIndex = new uint8_t[recordLengthsBytes];

        // Transform ints to bytes in local endian. It'll be swapped below in writeTrailer().
        for (int i = 0; i < recordLengths.size(); i++) {
            toBytes(recordLengths[i], ByteOrder::ENDIAN_LOCAL, recordIndex, 4*i, recordLengthsBytes);
//cout << "WriterMT::writeTrailer: writing record length = " << recordLengths[i] << showbase << hex <<
//                ", = " << recordLengths[i] << endl;
        }

        // Write trailer with index

        // How many bytes are we writing here?
        int dataBytes = RecordHeader::HEADER_SIZE_BYTES + recordLengthsBytes;

        // Make sure our array can hold everything
        if (headerArray.capacity() < dataBytes) {
//cout << "WriterMT::writeTrailer: allocating byte array of " << dataBytes << " bytes in size" << endl;
            headerArray.reserve(dataBytes);
        }

        // Place data into headerArray - both header and index
        RecordHeader::writeTrailer(&headerArray[0], dataBytes, 0,
                                   recordNumber, byteOrder, (const uint32_t *) recordIndex,
                                   recordLengthsBytes);

        writerBytesWritten += dataBytes;
        outFile.write(reinterpret_cast<const char *>(&headerArray[0]), dataBytes);
        if (outFile.fail()) {
            throw HipoException("error opening file " + fileName);
        }

        delete[] recordIndex;
    }

    // Find & update file header's trailer position word
    outFile.seekp(FileHeader::TRAILER_POSITION_OFFSET);
    if (byteOrder != ByteOrder::ENDIAN_LOCAL) {
        uint64_t pos = SWAP_64(trailerPosition);
        outFile.write(reinterpret_cast<const char *>(&pos), sizeof(uint64_t));
    }
    else {
        outFile.write(reinterpret_cast<const char *>(&trailerPosition), sizeof(uint64_t));
    }

    // Find & update file header's bit-info word
    if (writeIndex && addTrailerIndex) {
        outFile.seekp(RecordHeader::BIT_INFO_OFFSET);
        int bitInfo = fileHeader.getBitInfoWord();
        if (byteOrder != ByteOrder::ENDIAN_LOCAL) {
            uint32_t bitSwap = SWAP_32(bitInfo);
            outFile.write(reinterpret_cast<const char *>(&bitSwap), sizeof(uint32_t));
        }
        else {
            outFile.write(reinterpret_cast<const char *>(&bitInfo), sizeof(uint32_t));
        }
    }
}


/**
 * Appends the record to the file.
 * Using this method in conjunction with {@link #addEvent()} is not thread-safe.
 * @param rec record object
 * @throws HipoException if arg's byte order is opposite to output endian.
 */
void WriterMT::writeRecord(RecordOutput & rec) {

    // Need to ensure that the given record has a byte order the same as output
    if (rec.getByteOrder() != byteOrder) {
        throw HipoException("byte order of record is wrong");
    }

    // If we have already written stuff into our current internal record ...
    if (outputRecord->getEventCount() > 0) {
        // Put it back in supply for compressing
        supply->publish(ringItem);

        // Now get another, empty record.
        // This may block if threads are busy compressing
        // and/or writing all records in supply.
        ringItem = supply->get();
        outputRecord = ringItem->getRecord();
    }

    // Copy rec into an empty record taken from the supply
    outputRecord->transferDataForReading(rec);

    // Make sure given record is consistent with this writer
    RecordHeader & header = outputRecord->getHeader();
    header.setCompressionType(compressionType);
    header.setRecordNumber(recordNumber++);

    // Put it back in supply for compressing (building)
    supply->publish(ringItem);

    // Get another
    ringItem = supply->get();
    outputRecord = ringItem->getRecord();
}


/**
 * Add a byte array to the current internal record. If the length of
 * the buffer exceeds the maximum size of the record, the record
 * will be written to the file (compressed if the flag is set).
 * And another record will be obtained from the supply to receive the buffer.
 * Using this method in conjunction with {@link #writeRecord(RecordOutputStream)}
 * is not thread-safe.
 *
 * @param buffer array to add to the file.
 * @param offset offset into array from which to start writing data.
 * @param length number of bytes to write from array.
 */
void WriterMT::addEvent(uint8_t * buffer, uint32_t offset, uint32_t length) {
    // Try putting data into current record being filled
    bool status = outputRecord->addEvent(buffer, offset, length);

    // If record is full ...
    if (!status) {
        // Put it back in supply for compressing
        supply->publish(ringItem);

        // Now get another, empty record.
        // This may block if threads are busy compressing
        // and/or writing all records in supply.
        ringItem = supply->get();
        outputRecord = ringItem->getRecord();

        // Adding the first event to a record is guaranteed to work
        outputRecord->addEvent(buffer, offset, length);
    }
}


/**
 * Add a ByteBuffer to the internal record. If the length of
 * the buffer exceeds the maximum size of the record, the record
 * will be written to the file (compressed if the flag is set).
 * Internal record will be reset to receive new buffers.
 * Using this method in conjunction with writeRecord() is not thread-safe.
 * <b>The byte order of event's data must
 * match the byte order given in constructor!</b>
 *
 * @param buffer array to add to the file.
 * @throws HipoException if cannot write to file or buffer arg's byte order is wrong.
 */
void WriterMT::addEvent(ByteBuffer & buffer) {

    if (buffer.order() != byteOrder) {
        throw HipoException("buffer arg byte order is wrong");
    }

    bool status = outputRecord->addEvent(buffer);

    // If record is full ...
    if (!status) {
        // Put it back in supply for compressing
        supply->publish(ringItem);

        // Now get another, empty record.
        // This may block if threads are busy compressing
        // and/or writing all records in supply.
        ringItem = supply->get();
        outputRecord = ringItem->getRecord();

        // Adding the first event to a record is guaranteed to work
        outputRecord->addEvent(buffer);
    }
}


//---------------------------------------------------------------------

/** Get this object ready for re-use.
 * Follow calling this with call to {@link #open(String)}. */
void WriterMT::reset() {
    outputRecord->reset();
    fileHeader.reset();
    writerBytesWritten = 0L;
    recordNumber = 1;
    addingTrailer = false;
}


/**
 * Close opened file. If the output record contains events,
 * they will be flushed to file. Trailer and its optional index
 * written if requested.<p>
 * <b>The addEvent or addRecord methods must no longer be called.</b>
 */
void WriterMT::close() {

    // If we're in the middle of building a record, send it off since we're done
    if (outputRecord->getEventCount() > 0) {
        // Put it in queue for compressing
        supply->publish(ringItem);
    }

    // Since the writer thread is the last to process each record,
    // wait until it's done with the last item, then exit the thread.
    recordWriterThreads[0].waitForLastItem();

    // Stop all compressing threads
    for (RecordCompressor &thd : recordCompressorThreads) {
        thd.stopThread();
    }

    // Don't hang on to thread objects and therefore boost threads & RecordSupply objects
    recordCompressorThreads.clear();
    recordWriterThreads.clear();

    try {
        if (addingTrailer) {
            // Write the trailer
            writeTrailer(addTrailerIndex);
        }

        // Need to update the record count in file header
        outFile.seekp(FileHeader::RECORD_COUNT_OFFSET);
        if (byteOrder == ByteOrder::ENDIAN_LITTLE) {
            uint32_t bitSwap = SWAP_32(recordNumber - 1);
            outFile.write(reinterpret_cast<const char *>(&bitSwap), sizeof(uint32_t));
        } else {
            uint32_t intData = recordNumber - 1;
            outFile.write(reinterpret_cast<const char *>(&intData), sizeof(uint32_t));
        }
        outFile.close();
    }
    catch (HipoException & ex) {
        cout << "WriterMT::close ERROR!!!" << endl;
    }

    ringItem = nullptr;
    closed = true;
    opened = false;
}
