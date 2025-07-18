// -----------------------------------------------------------------------------
//
// Copyright (C) 2021 CERN & University of Surrey for the benefit of the
// BioDynaMo collaboration. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------

#ifndef CORE_UTIL_SERIALIZATION_STD_H_
#define CORE_UTIL_SERIALIZATION_STD_H_

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <fstream>
#include <memory>

namespace bdm {
namespace experimental {

/// RAII wrapper for file operations - alternative to TFileRaii
class FileRaii {
 public:
  enum Mode { READ, WRITE, APPEND };
  
  FileRaii(const std::string& filename, Mode mode) : filename_(filename) {
    switch (mode) {
      case READ:
        if_stream_ = std::make_unique<std::ifstream>(filename, std::ios::binary);
        if (!if_stream_->is_open()) {
          throw std::runtime_error("Cannot open file for reading: " + filename);
        }
        break;
      case WRITE:
        of_stream_ = std::make_unique<std::ofstream>(filename, std::ios::binary);
        if (!of_stream_->is_open()) {
          throw std::runtime_error("Cannot open file for writing: " + filename);
        }
        break;
      case APPEND:
        of_stream_ = std::make_unique<std::ofstream>(filename, std::ios::binary | std::ios::app);
        if (!of_stream_->is_open()) {
          throw std::runtime_error("Cannot open file for appending: " + filename);
        }
        break;
    }
  }
  
  ~FileRaii() = default;
  
  std::ifstream& GetInputStream() {
    if (!if_stream_) {
      throw std::runtime_error("File not opened for reading");
    }
    return *if_stream_;
  }
  
  std::ofstream& GetOutputStream() {
    if (!of_stream_) {
      throw std::runtime_error("File not opened for writing");
    }
    return *of_stream_;
  }
  
  bool IsValid() const {
    return (if_stream_ && if_stream_->is_open()) || (of_stream_ && of_stream_->is_open());
  }

 private:
  std::string filename_;
  std::unique_ptr<std::ifstream> if_stream_;
  std::unique_ptr<std::ofstream> of_stream_;
};

/// Alternative to ROOT's WritePersistentObject using Boost Serialization
template <typename T>
void WriteBoostObject(const std::string& filename, const std::string& obj_name, 
                      const T& object, bool use_binary = true) {
  try {
    FileRaii file(filename, FileRaii::WRITE);
    
    if (use_binary) {
      boost::archive::binary_oarchive oa(file.GetOutputStream());
      oa << obj_name << object;
    } else {
      boost::archive::text_oarchive oa(file.GetOutputStream());
      oa << obj_name << object;
    }
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to write object '" + obj_name + 
                             "' to file '" + filename + "': " + e.what());
  }
}

/// Alternative to ROOT's GetPersistentObject using Boost Serialization
template <typename T>
bool ReadBoostObject(const std::string& filename, const std::string& obj_name,
                     T& object, bool use_binary = true) {
  try {
    FileRaii file(filename, FileRaii::READ);
    std::string stored_name;
    
    if (use_binary) {
      boost::archive::binary_iarchive ia(file.GetInputStream());
      ia >> stored_name >> object;
    } else {
      boost::archive::text_iarchive ia(file.GetInputStream());
      ia >> stored_name >> object;
    }
    
    if (stored_name != obj_name) {
      throw std::runtime_error("Object name mismatch. Expected: " + obj_name + 
                               ", Found: " + stored_name);
    }
    
    return true;
  } catch (const std::exception& e) {
    // Log error but don't throw - match ROOT behavior
    std::cerr << "Failed to read object '" << obj_name << 
                 "' from file '" << filename << "': " << e.what() << std::endl;
    return false;
  }
}

/// Simple wrapper for integral types (alternative to ROOT's IntegralTypeWrapper)
template <typename T>
class SimpleWrapper {
 public:
  SimpleWrapper() = default;
  explicit SimpleWrapper(const T& data) : data_(data) {}
  
  const T& Get() const { return data_; }
  void Set(const T& data) { data_ = data; }
  
  // Boost serialization
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & data_;
  }

 private:
  T data_{};
};

/// Runtime system information (alternative to ROOT's RuntimeVariables)
class SystemInfo {
 public:
  SystemInfo() {
    // Collect basic system information
    hostname_ = GetHostname();
    timestamp_ = GetCurrentTimestamp();
    pid_ = GetProcessId();
  }
  
  bool operator==(const SystemInfo& other) const {
    return hostname_ == other.hostname_ && 
           pid_ == other.pid_;
    // Note: Don't compare timestamp for equality
  }
  
  bool operator!=(const SystemInfo& other) const {
    return !(*this == other);
  }
  
  void Print() const {
    std::cout << "System Information:\n";
    std::cout << "  Hostname: " << hostname_ << "\n";
    std::cout << "  Process ID: " << pid_ << "\n";
    std::cout << "  Timestamp: " << timestamp_ << "\n";
  }
  
  // Boost serialization
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & hostname_ & timestamp_ & pid_;
  }

 private:
  std::string hostname_;
  std::string timestamp_;
  int pid_;
  
  std::string GetHostname() const {
    char buffer[256];
    if (gethostname(buffer, sizeof(buffer)) == 0) {
      return std::string(buffer);
    }
    return "unknown";
  }
  
  std::string GetCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    return std::ctime(&time_t);
  }
  
  int GetProcessId() const {
    return getpid();
  }
};

/// Simple backup and restore functionality
template <typename T>
class SimpleBackup {
 public:
  SimpleBackup(const std::string& backup_file, const std::string& restore_file = "")
    : backup_file_(backup_file), restore_file_(restore_file) {}
  
  void BackupObject(const T& object, const std::string& obj_name) {
    if (backup_file_.empty()) {
      throw std::runtime_error("No backup file specified");
    }
    
    WriteBoostObject(backup_file_, obj_name, object);
    
    // Also save system info
    SystemInfo sysinfo;
    WriteBoostObject(backup_file_ + ".sysinfo", "system_info", sysinfo);
  }
  
  bool RestoreObject(T& object, const std::string& obj_name) {
    if (restore_file_.empty()) {
      throw std::runtime_error("No restore file specified");
    }
    
    // Check system info compatibility
    SystemInfo current_sysinfo;
    SystemInfo stored_sysinfo;
    if (ReadBoostObject(restore_file_ + ".sysinfo", "system_info", stored_sysinfo)) {
      if (current_sysinfo != stored_sysinfo) {
        std::cerr << "Warning: Restoring from different system!\n";
        current_sysinfo.Print();
        std::cout << "Stored system info:\n";
        stored_sysinfo.Print();
      }
    }
    
    return ReadBoostObject(restore_file_, obj_name, object);
  }

 private:
  std::string backup_file_;
  std::string restore_file_;
};

/// Macro to help with Boost serialization (similar to BDM_CLASS_DEF)
#define BDM_BOOST_SERIALIZABLE(ClassName) \
  template <class Archive> \
  void serialize(Archive& ar, const unsigned int version)

/// Utility functions
inline bool FileExists(const std::string& filename) {
  std::ifstream file(filename);
  return file.good();
}

inline void RemoveFile(const std::string& filename) {
  std::remove(filename.c_str());
}

} // namespace experimental
} // namespace bdm

#endif // CORE_UTIL_SERIALIZATION_STD_H_
