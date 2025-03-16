#include "io.hpp"
#include "string.hpp"
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace utils::io
{
	bool remove_file(const std::string& file)
	{
#ifdef _WIN32
		if (DeleteFileA(file.data()) != FALSE)
		{
			return true;
		}
		return GetLastError() == ERROR_FILE_NOT_FOUND;
#else
		return (unlink(file.c_str()) == 0 || errno == ENOENT);
#endif
	}

	bool move_file(const std::string& src, const std::string& target)
	{
#ifdef _WIN32
		return MoveFileA(src.data(), target.data()) == TRUE;
#else
		return rename(src.c_str(), target.c_str()) == 0;
#endif
	}

	bool read_file(const std::string& file, std::string* data)
	{
		if (!data) return false;

		std::ifstream stream(file, std::ios::binary);
		if (!stream.is_open()) return false;

		stream.seekg(0, std::ios::end);
		const auto size = stream.tellg();
		stream.seekg(0, std::ios::beg);

		if (size == -1) return false;

		data->resize(static_cast<size_t>(size));
		stream.read(data->data(), size);

		return true;
	}

	bool write_file(const std::string& file, const std::string& data, const bool append)
	{
		std::ofstream stream(file, 
			std::ios::binary | 
			(append ? std::ios::app : std::ios::out));

		if (!stream.is_open()) return false;

		stream.write(data.data(), static_cast<std::streamsize>(data.size()));
		return true;
	}

	bool file_exists(const std::string& file)
	{
		return std::filesystem::exists(file);
	}

	bool create_directory(const std::string& directory)
	{
		return std::filesystem::create_directories(directory);
	}

	std::string get_temporary_directory()
	{
		return std::filesystem::temp_directory_path().string();
	}

	std::string get_current_directory()
	{
		return std::filesystem::current_path().string();
	}

	bool set_current_directory(const std::string& directory)
	{
		std::error_code ec;
		std::filesystem::current_path(directory, ec);
		return !ec;
	}

	std::string file_name(const std::string& path)
	{
		const auto pos = path.find_last_of('/');
		if (pos == std::string::npos) return path;

		return path.substr(pos + 1);
	}

	std::string file_extension(const std::string& path)
	{
		const auto pos = path.find_last_of('.');
		if (pos == std::string::npos) return "";

		return path.substr(pos + 1);
	}

	size_t file_size(const std::string& file)
	{
		if (file_exists(file))
		{
			std::ifstream stream(file, std::ios::binary);

			if (stream.good())
			{
				stream.seekg(0, std::ios::end);
				return static_cast<size_t>(stream.tellg());
			}
		}

		return 0;
	}

	time_t file_timestamp(const std::string& file)
	{
		return std::filesystem::last_write_time(file).time_since_epoch().count();
	}

	bool directory_exists(const std::string& directory)
	{
		return std::filesystem::is_directory(directory);
	}

	bool directory_is_empty(const std::string& directory)
	{
		return std::filesystem::is_empty(directory);
	}

	std::vector<std::string> list_files(const std::string& directory)
	{
		std::vector<std::string> files;

		for (auto& file : std::filesystem::directory_iterator(directory))
		{
			if (std::filesystem::is_directory(file.path())) continue; //exclude directory from output

			files.push_back(file.path().generic_string());
		}

		return files;
	}

	void copy_folder(const std::filesystem::path& src, const std::filesystem::path& target)
	{
		std::filesystem::copy(src, target,
			std::filesystem::copy_options::overwrite_existing |
			std::filesystem::copy_options::recursive);
	}

	std::string read_file(const std::string& file)
	{
		std::string data;
		read_file(file, &data);
		return data;
	}
}
