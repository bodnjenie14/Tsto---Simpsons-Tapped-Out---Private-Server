#include "compression.hpp"
#include <zlib.h>
#ifdef _WIN32
#include <zip.h>
#include <unzip.h>
#else
#include <zip.h>  // On Linux, it's just <zip.h>
#endif
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "io.hpp"
#include "finally.hpp"

namespace utils::compression
{
    namespace zlib
    {
        std::string decompress(const std::string& data, bool is_gzip) {
            if (data.empty()) {
                return "";
            }

            z_stream stream{};
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;
            stream.avail_in = static_cast<uInt>(data.size());
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));

            // Use RAII to ensure proper cleanup
            auto cleanup = finally([&stream]() {
                inflateEnd(&stream);
            });

            // Initialize with gzip format if specified
            int window_bits = 15 + (is_gzip ? 16 : 0);
            if (inflateInit2(&stream, window_bits) != Z_OK) {
                throw std::runtime_error("Failed to initialize zlib inflate");
            }

            std::string result;
            std::vector<char> buffer(CHUNK);

            do {
                stream.avail_out = static_cast<uInt>(buffer.size());
                stream.next_out = reinterpret_cast<Bytef*>(buffer.data());

                int ret = inflate(&stream, Z_NO_FLUSH);
                if (ret != Z_OK && ret != Z_STREAM_END) {
                    // Add more detailed error information
                    char error_msg[256];
                    if (stream.msg) {
                        snprintf(error_msg, sizeof(error_msg), "Failed to decompress data: %s", stream.msg);
                    } else {
                        snprintf(error_msg, sizeof(error_msg), "Failed to decompress data (error code: %d)", ret);
                    }
                    throw std::runtime_error(error_msg);
                }

                size_t have = buffer.size() - stream.avail_out;
                result.append(buffer.data(), have);

            } while (stream.avail_out == 0);

            return result;
        }

        std::string decompress_gzip(const std::string& data) {
            return decompress(data, true);
        }

        std::string compress(const std::string& data) {
            if (data.empty()) {
                return "";
            }

            z_stream stream{};
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;

            // Use RAII to ensure proper cleanup
            auto cleanup = finally([&stream]() {
                deflateEnd(&stream);
            });

            if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK) {
                throw std::runtime_error("Failed to initialize zlib deflate");
            }

            stream.avail_in = static_cast<uInt>(data.size());
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));

            std::string result;
            std::vector<char> buffer(CHUNK);

            do {
                stream.avail_out = static_cast<uInt>(buffer.size());
                stream.next_out = reinterpret_cast<Bytef*>(buffer.data());

                int ret = deflate(&stream, Z_FINISH);
                if (ret != Z_OK && ret != Z_STREAM_END) {
                    throw std::runtime_error("Failed to compress data");
                }

                size_t have = buffer.size() - stream.avail_out;
                result.append(buffer.data(), have);

            } while (stream.avail_out == 0);

            return result;
        }
    }

    namespace zip
    {
#ifdef _WIN32
        // Windows-specific implementation using minizip
        bool add_file(zipFile& zip_file, const std::string& filename, const std::string& data)
        {
            const auto zip_64 = data.size() > 0xffffffff ? 1 : 0;
            if (ZIP_OK != zipOpenNewFileInZip64(zip_file, filename.data(), nullptr, nullptr, 0, nullptr, 0, nullptr,
                                            Z_DEFLATED, Z_BEST_COMPRESSION, zip_64))
            {
                return false;
            }

            const auto _ = finally([&zip_file]()
            {
                zipCloseFileInZip(zip_file);
            });

            return ZIP_OK == zipWriteInFileInZip(zip_file, data.data(), static_cast<unsigned>(data.size()));
        }
#endif

        void archive::add(std::string filename, std::string data)
        {
            // Convert Windows paths to Unix style for zip
            std::replace(filename.begin(), filename.end(), '\\', '/');
            files_[std::move(filename)] = std::move(data);
        }

        bool archive::write(const std::string& filename, const std::string& comment)
        {
            try
            {
                // Create all parent directories
                std::filesystem::path path(filename);
                std::filesystem::create_directories(path.parent_path());

#ifdef _WIN32
                // Windows implementation using minizip
                auto* zip_file = zipOpen64(filename.data(), 0);
                if (!zip_file)
                {
                    return false;
                }

                const auto _ = finally([&zip_file, &comment]()
                {
                    zipClose(zip_file, comment.empty() ? nullptr : comment.data());
                });

                for (const auto& file : files_)
                {
                    if (!add_file(zip_file, file.first, file.second))
                    {
                        return false;
                    }
                }
#else
                // Linux implementation using libzip
                int error = 0;
                zip_t* archive = zip_open(filename.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
                if (!archive) {
                    return false;
                }

                const auto _ = finally([&archive]() {
                    zip_close(archive);
                });

                for (const auto& file : files_) {
                    zip_source_t* source = zip_source_buffer(archive, file.second.data(), file.second.size(), 0);
                    if (!source) {
                        return false;
                    }

                    if (zip_file_add(archive, file.first.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
                        zip_source_free(source);
                        return false;
                    }
                }

                if (!comment.empty()) {
                    zip_set_archive_comment(archive, comment.c_str(), comment.length());
                }
#endif
                return true;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        std::unordered_map<std::string, std::string> extract(const std::string& data)
        {
            std::unordered_map<std::string, std::string> files;
            
            // Create a temporary file to store the zip data
            std::filesystem::path temp_path = std::filesystem::temp_directory_path() / "temp.zip";
            {
                std::ofstream temp_file(temp_path, std::ios::binary);
                temp_file.write(data.data(), data.size());
            }

            const auto cleanup = finally([&temp_path]() {
                std::filesystem::remove(temp_path);
            });

#ifdef _WIN32
            // Windows implementation using minizip
            auto* zip_file = unzOpen64(temp_path.string().c_str());
            if (!zip_file) {
                return files;
            }

            const auto zip_cleanup = finally([&zip_file]() {
                unzClose(zip_file);
            });

            unz_global_info64 global_info;
            if (unzGetGlobalInfo64(zip_file, &global_info) != UNZ_OK) {
                return files;
            }

            std::vector<char> buffer(CHUNK);
            for (uLong i = 0; i < global_info.number_entry; ++i) {
                char filename[256];
                unz_file_info64 file_info;

                if (unzGetCurrentFileInfo64(zip_file, &file_info, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK) {
                    continue;
                }

                if (unzOpenCurrentFile(zip_file) != UNZ_OK) {
                    continue;
                }

                std::string content;
                int err;
                do {
                    err = unzReadCurrentFile(zip_file, buffer.data(), buffer.size());
                    if (err < 0) {
                        break;
                    }
                    if (err > 0) {
                        content.append(buffer.data(), err);
                    }
                } while (err > 0);

                unzCloseCurrentFile(zip_file);

                if (err == 0) {
                    files[filename] = std::move(content);
                }

                if ((i + 1) < global_info.number_entry) {
                    if (unzGoToNextFile(zip_file) != UNZ_OK) {
                        break;
                    }
                }
            }
#else
            // Linux implementation using libzip
            int error = 0;
            zip_t* archive = zip_open(temp_path.c_str(), 0, &error);
            if (!archive) {
                return files;
            }

            const auto zip_cleanup = finally([&archive]() {
                zip_close(archive);
            });

            zip_int64_t num_entries = zip_get_num_entries(archive, 0);
            for (zip_int64_t i = 0; i < num_entries; i++) {
                const char* name = zip_get_name(archive, i, 0);
                if (!name) continue;

                zip_file_t* file = zip_fopen_index(archive, i, 0);
                if (!file) continue;

                const auto file_cleanup = finally([&file]() {
                    zip_fclose(file);
                });

                std::string content;
                std::vector<char> buffer(CHUNK);
                zip_int64_t len;
                while ((len = zip_fread(file, buffer.data(), buffer.size())) > 0) {
                    content.append(buffer.data(), len);
                }

                files[name] = std::move(content);
            }
#endif
            return files;
        }
    }
}
