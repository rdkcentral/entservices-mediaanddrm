/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DemuxerStreamFsFCC.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <boost/algorithm/string.hpp>

// delimiter used for string tokenizing
#define DELIMITER ','
// index definitions for seek fsnode
#define CURRENT_SEEK_IN_SEC 0
#define CURRENT_SIZE_IN_SEC 1
#define CURRENT_SEEK_IN_BYTES 2
#define CURRENT_SIZE_IN_BYTES 3
#define MAX_SIZE_IN_BYTES 4
// index definitions for stream_status fsnode
#define STREAM_SOURCE_LOST 0
#define STREAM_SOURCE_LOSS_COUNT 1

namespace WPEFramework {
namespace Plugin {

// class helper functions declared in anonymous namespace
namespace {

template<typename T>
inline std::string numberToString(T value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

template<typename T>
inline bool stringToNumber(const std::string &in, T &out) {
    std::istringstream is(in);
    is >> out;
    return !(in.empty() || numberToString<T>(out) != in);
}

IDemuxer::IO_STATUS read(const std::string &name, std::string &data) {
    std::cout << "naveen: read() called, file=" << name << std::endl;
    std::ifstream file(name);
    syslog(LOG_INFO, "Reading from file: %s", name.c_str());
    if (!file.is_open()) {
        return DemuxerStreamFsFCC::IO_STATUS::READ_ERROR;
    }
    getline(file, data);
    std::cout << "naveen: read() got data='" << data << "' from file=" << name << std::endl;
    file.close();
    return DemuxerStreamFsFCC::IO_STATUS::OK;
}

IDemuxer::IO_STATUS write(const std::string &name, const std::string &data) {
    std::cout << "naveen: write() called, file=" << name << ", data='" << data << "'" << std::endl;
    std::ofstream file(name);
    syslog(LOG_ERR, "Writing to file: %s", name.c_str());
    if (!file.is_open()) {
        return DemuxerStreamFsFCC::IO_STATUS::WRITE_ERROR;
    }
    file << data;
    file.close();
    if (!file) {
        // return WRITE_ERROR if file system reported error during writing of the data
        return DemuxerStreamFsFCC::IO_STATUS::WRITE_ERROR;
    }
    return DemuxerStreamFsFCC::IO_STATUS::OK;
}

template<typename T>
std::vector<T> arrayFromString(std::string input, unsigned char delim) {
    std::transform(input.begin(), input.end(), input.begin(), [&delim](unsigned char c) -> unsigned char { return c == delim ? ' ' : c;});
    std::stringstream ss(input);
    std::vector<T> output;
    std::copy(std::istream_iterator<T>(ss), std::istream_iterator<T>(), std::back_inserter(output));
    return output;
}

template<typename T>
IDemuxer::IO_STATUS readArray(const std::string &name, std::vector<T> &array, size_t expectedSize) {
    std::string line;
    if (read(name, line) == DemuxerStreamFsFCC::IO_STATUS::OK) {
        array = arrayFromString<T>(line, DELIMITER);
        if (array.size() == expectedSize) {
            return DemuxerStreamFsFCC::IO_STATUS::OK;
        }
        return DemuxerStreamFsFCC::IO_STATUS::PARSE_ERROR;
    }
    return DemuxerStreamFsFCC::IO_STATUS::READ_ERROR;
}

} // namespace anonymous


IDemuxer::IO_STATUS DemuxerStreamFsFCC::open()
{
    std::cout << "naveen: DemuxerStreamFsFCC::open() called" << std::endl;
    // implementation to be added later
    return DemuxerStreamFsFCC::IO_STATUS::OK;
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::close()
{
    std::cout << "naveen: DemuxerStreamFsFCC::close() called" << std::endl;
    // implementation to be added later
    return DemuxerStreamFsFCC::IO_STATUS::OK;
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::setChannel(const std::string& channel)
{
    std::cout << "naveen: DemuxerStreamFsFCC::setChannel() called, file=" << DemuxerStreamFsFCC::_fileChannel << ", channel='" << channel << "'" << std::endl;
    return write(DemuxerStreamFsFCC::_fileChannel, channel);
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::getChannel(std::string& channel)
{
    std::cout << "naveen: DemuxerStreamFsFCC::getChannel() called, file=" << DemuxerStreamFsFCC::_fileChannel << std::endl;
    return read(DemuxerStreamFsFCC::_fileChannel, channel);
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::setSeekPosInSeconds(uint64_t seekSeconds)
{
    std::cout << "naveen: DemuxerStreamFsFCC::setSeekPosInSeconds() called, file=" << DemuxerStreamFsFCC::_fileSeek << ", seekSeconds=" << seekSeconds << std::endl;
    return write(DemuxerStreamFsFCC::_fileSeek, numberToString<uint64_t>(seekSeconds));
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::getSeek(SeekType &result) {
    std::cout << "naveen: DemuxerStreamFsFCC::getSeek() called, file=" << DemuxerStreamFsFCC::_fileSeek << std::endl;
    std::vector<uint64_t> values;
    auto status = readArray<uint64_t>(DemuxerStreamFsFCC::_fileSeek, values, 5);
    if (status == IO_STATUS::OK) {
        result.seekPosInSeconds      = values[CURRENT_SEEK_IN_SEC];
        result.seekPosInBytes        = values[CURRENT_SEEK_IN_BYTES];
        result.currentSizeInSeconds  = values[CURRENT_SIZE_IN_SEC];
        result.currentSizeInBytes    = values[CURRENT_SIZE_IN_BYTES];
        result.maxSizeInBytes        = values[MAX_SIZE_IN_BYTES];
        std::cout << "naveen: getSeek() values: seekPosInSeconds=" << result.seekPosInSeconds << ", seekPosInBytes=" << result.seekPosInBytes << ", currentSizeInSeconds=" << result.currentSizeInSeconds << ", currentSizeInBytes=" << result.currentSizeInBytes << ", maxSizeInBytes=" << result.maxSizeInBytes << std::endl;
    } else {
        std::cout << "naveen: getSeek() failed with status=" << status << std::endl;
    }
    return status;
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::setTrickPlaySpeed(int16_t speed)
{
    std::cout << "naveen: DemuxerStreamFsFCC::setTrickPlaySpeed() called, file=" << DemuxerStreamFsFCC::_fileTrickPlay << ", speed=" << speed << std::endl;
    return write(DemuxerStreamFsFCC::_fileTrickPlay, numberToString<int16_t>(speed));
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::getTrickPlaySpeed(int16_t &result)
{
    std::cout << "naveen: DemuxerStreamFsFCC::getTrickPlaySpeed() called, file=" << DemuxerStreamFsFCC::_fileTrickPlay << std::endl;
    std::string line;
    if (read(DemuxerStreamFsFCC::_fileTrickPlay, line) == DemuxerStreamFsFCC::IO_STATUS::OK) {
        if (stringToNumber<int16_t>(line, result)) {
            std::cout << "naveen: getTrickPlaySpeed() got speed=" << result << std::endl;
            return DemuxerStreamFsFCC::IO_STATUS::OK;
        }
        std::cout << "naveen: getTrickPlaySpeed() parse error" << std::endl;
        return DemuxerStreamFsFCC::IO_STATUS::PARSE_ERROR;
    }
    std::cout << "naveen: getTrickPlaySpeed() read error" << std::endl;
    return DemuxerStreamFsFCC::IO_STATUS::READ_ERROR;
}

IDemuxer::IO_STATUS DemuxerStreamFsFCC::getStreamStatus(StreamStatusType &result)
{
    std::cout << "naveen: DemuxerStreamFsFCC::getStreamStatus() called, file=" << DemuxerStreamFsFCC::_fileStatus << std::endl;
    std::vector<uint64_t> values;
    auto status = readArray<uint64_t>(DemuxerStreamFsFCC::_fileStatus, values, 2);
    if (status == IO_STATUS::OK) {
        result.streamSourceLost      = values[STREAM_SOURCE_LOST];
        result.streamSourceLossCount = values[STREAM_SOURCE_LOSS_COUNT];
        std::cout << "naveen: getStreamStatus() values: streamSourceLost=" << result.streamSourceLost << ", streamSourceLossCount=" << result.streamSourceLossCount << std::endl;
    } else {
        std::cout << "naveen: getStreamStatus() failed with status=" << status << std::endl;
    }
    return status;
}

std::string DemuxerStreamFsFCC::getTrickPlayFile() const {
    std::cout << "naveen: DemuxerStreamFsFCC::getTrickPlayFile() called" << std::endl;
    return _fileTrickPlay;
}

} // namespace Plugin
} // namespace WPEFramework

