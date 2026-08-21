#include "far.hpp"
#include "../util/utils.hpp"
#include "../util/common.hpp"
#include "bytecode.hpp"
#include <cstring>
#include <sys/stat.h>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

// --- ZIP signatures ---
#define ZIP_LOCAL_FILE_SIG      0x04034b50
#define ZIP_CENTRAL_DIR_SIG     0x02014b50
#define ZIP_EOCD_SIG            0x06054b50

// --- LE helpers ---
static void write16le(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

static void write32le(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

static uint16_t read16le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// --- CRC-32 ---
static uint32_t crc32_tab[256];
static bool crc32_init = false;

static void ensure_crc32() {
    if (crc32_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320) : (crc >> 1);
        crc32_tab[i] = crc;
    }
    crc32_init = true;
}

static uint32_t calc_crc32(const uint8_t* data, size_t len) {
    ensure_crc32();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc32_tab[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

// --- DOS date/time helper ---
static void dos_datetime(uint16_t& dos_time, uint16_t& dos_date) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetSystemTime(&st);
    dos_time = static_cast<uint16_t>((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond >> 1));
    dos_date = static_cast<uint16_t>(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);
#else
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    dos_time = static_cast<uint16_t>((t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec >> 1));
    dos_date = static_cast<uint16_t>(((t->tm_year) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday);
#endif
}

// ============================================================
// FarArchive implementation (ZIP format)
// ============================================================

bool FarArchive::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open archive: " << path << std::endl;
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    entries.clear();

    if (data.size() < 22) {
        std::cerr << "Invalid .far file: too small" << std::endl;
        return false;
    }

    if (read32le(data.data()) != ZIP_LOCAL_FILE_SIG) {
        std::cerr << "Invalid .far file: not a ZIP archive" << std::endl;
        return false;
    }

    // Find EOCD (search backwards for PK\x05\x06)
    size_t eocd_pos = (data.size() >= 65557) ? data.size() - 65557 : 0;
    bool found = false;
    for (; eocd_pos + 22 <= data.size(); eocd_pos++) {
        if (read32le(data.data() + eocd_pos) == ZIP_EOCD_SIG) {
            found = true;
            break;
        }
    }
    if (!found) {
        std::cerr << "Invalid .far file: no EOCD record" << std::endl;
        return false;
    }

    const uint8_t* eocd = data.data() + eocd_pos;
    uint16_t num_entries  = read16le(eocd + 8);
    uint32_t cd_size      = read32le(eocd + 12);
    uint32_t cd_offset    = read32le(eocd + 16);

    if (cd_offset + cd_size > data.size()) {
        std::cerr << "Corrupt .far: central directory out of bounds" << std::endl;
        return false;
    }

    size_t cd_pos = cd_offset;
    for (uint16_t i = 0; i < num_entries; i++) {
        if (cd_pos + 46 > data.size()) break;
        const uint8_t* cd_entry = data.data() + cd_pos;

        if (read32le(cd_entry) != ZIP_CENTRAL_DIR_SIG) {
            std::cerr << "Corrupt .far: bad central directory signature" << std::endl;
            return false;
        }

        uint16_t name_len    = read16le(cd_entry + 28);
        uint16_t extra_len   = read16le(cd_entry + 30);
        uint16_t comment_len = read16le(cd_entry + 32);
        uint32_t comp_size   = read32le(cd_entry + 20);
        uint32_t local_off   = read32le(cd_entry + 42);

        std::string name(reinterpret_cast<const char*>(cd_entry + 46), name_len);

        // Read local file header to get file data
        if (local_off + 30 > data.size()) {
            std::cerr << "Corrupt .far: local header out of bounds" << std::endl;
            return false;
        }
        const uint8_t* local = data.data() + local_off;
        if (read32le(local) != ZIP_LOCAL_FILE_SIG) {
            std::cerr << "Corrupt .far: bad local header signature" << std::endl;
            return false;
        }
        uint16_t lname_len  = read16le(local + 26);
        uint16_t lextra_len = read16le(local + 28);

        size_t data_start = local_off + 30 + lname_len + lextra_len;
        if (data_start + comp_size > data.size()) {
            std::cerr << "Corrupt .far: file data out of bounds" << std::endl;
            return false;
        }

        FarEntry entry;
        entry.name = name;
        entry.data.assign(data.data() + data_start, data.data() + data_start + comp_size);
        entries.push_back(entry);

        cd_pos += 46 + name_len + extra_len + comment_len;
    }

    std::cout << "Loaded " << entries.size() << " entries from " << path << std::endl;
    return true;
}

bool FarArchive::save(const std::string& path) const {
    std::vector<uint8_t> archive;
    std::vector<uint8_t> central_dir;
    std::vector<uint32_t> local_offsets;

    uint16_t dos_time, dos_date;
    dos_datetime(dos_time, dos_date);

    // --- Local file entries ---
    for (const auto& entry : entries) {
        local_offsets.push_back(static_cast<uint32_t>(archive.size()));

        uint32_t crc  = calc_crc32(entry.data.data(), entry.data.size());
        uint32_t size = static_cast<uint32_t>(entry.data.size());

        write32le(archive, ZIP_LOCAL_FILE_SIG);
        write16le(archive, 20);        // version needed
        write16le(archive, 0);         // flags
        write16le(archive, 0);         // compression: stored
        write16le(archive, dos_time);
        write16le(archive, dos_date);
        write32le(archive, crc);
        write32le(archive, size);      // compressed size
        write32le(archive, size);      // uncompressed size
        write16le(archive, static_cast<uint16_t>(entry.name.size()));
        write16le(archive, 0);         // extra field length
        archive.insert(archive.end(), entry.name.begin(), entry.name.end());
        archive.insert(archive.end(), entry.data.begin(), entry.data.end());
    }

    // --- Central directory ---
    uint32_t cd_offset = static_cast<uint32_t>(archive.size());
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& entry = entries[i];
        uint32_t crc  = calc_crc32(entry.data.data(), entry.data.size());
        uint32_t size = static_cast<uint32_t>(entry.data.size());

        write32le(central_dir, ZIP_CENTRAL_DIR_SIG);
        write16le(central_dir, 20);     // version made by
        write16le(central_dir, 20);     // version needed
        write16le(central_dir, 0);      // flags
        write16le(central_dir, 0);      // compression: stored
        write16le(central_dir, dos_time);
        write16le(central_dir, dos_date);
        write32le(central_dir, crc);
        write32le(central_dir, size);   // compressed size
        write32le(central_dir, size);   // uncompressed size
        write16le(central_dir, static_cast<uint16_t>(entry.name.size()));
        write16le(central_dir, 0);      // extra field length
        write16le(central_dir, 0);      // comment length
        write16le(central_dir, 0);      // disk number start
        write16le(central_dir, 0);      // internal file attrs
        write32le(central_dir, 0);      // external file attrs
        write32le(central_dir, local_offsets[i]);
        central_dir.insert(central_dir.end(), entry.name.begin(), entry.name.end());
    }

    archive.insert(archive.end(), central_dir.begin(), central_dir.end());

    // --- EOCD ---
    write32le(archive, ZIP_EOCD_SIG);
    write16le(archive, 0);                               // disk number
    write16le(archive, 0);                               // disk with CD
    write16le(archive, static_cast<uint16_t>(entries.size()));
    write16le(archive, static_cast<uint16_t>(entries.size()));
    write32le(archive, static_cast<uint32_t>(central_dir.size()));
    write32le(archive, cd_offset);
    write16le(archive, 0);                               // comment length

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot create archive: " << path << std::endl;
        return false;
    }
    file.write(reinterpret_cast<const char*>(archive.data()), archive.size());
    file.close();

    std::cout << "Created " << path << " (" << archive.size() << " bytes, " << entries.size() << " entries)" << std::endl;
    return true;
}

void FarArchive::addFile(const std::string& name, const std::vector<uint8_t>& data) {
    entries.push_back({name, data});
}

bool FarArchive::extractFile(const std::string& name, std::vector<uint8_t>& data) const {
    for (const auto& entry : entries) {
        if (entry.name == name) {
            data = entry.data;
            return true;
        }
    }
    return false;
}

bool FarArchive::hasFile(const std::string& name) const {
    for (const auto& entry : entries) {
        if (entry.name == name) return true;
    }
    return false;
}

// Reject absolute paths, drive letters and ".." segments (P3-2, Zip-slip)
static bool isSafeEntryName(const std::string& name) {
    if (name.empty()) return false;
    if (name[0] == '/' || name[0] == '\\') return false;
    if (name.size() >= 2 && name[1] == ':') return false; // Windows drive letter
    std::string norm = name;
    for (char& c : norm) {
        if (c == '\\') c = '/';
    }
    size_t start = 0;
    while (true) {
        size_t end = norm.find('/', start);
        std::string seg = (end == std::string::npos) ? norm.substr(start) : norm.substr(start, end - start);
        if (seg == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

bool FarArchive::extractAll(const std::string& outputDir) const {
    // Create output directory
    mkdir(outputDir.c_str());

    size_t extracted = 0;
    for (const auto& entry : entries) {
        if (!isSafeEntryName(entry.name)) {
            std::cerr << "Skipping unsafe entry name: " << entry.name << std::endl;
            continue;
        }
        extracted++;
        std::string filePath = outputDir + "/" + entry.name;
        // Create subdirectories if needed
        size_t pos = 0;
        while ((pos = filePath.find_first_of("/\\", pos + 1)) != std::string::npos) {
            std::string dir = filePath.substr(0, pos);
            mkdir(dir.c_str());
        }

        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Cannot extract: " << filePath << std::endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());
        file.close();
        std::cout << "  extracted: " << filePath << " (" << entry.data.size() << " bytes)" << std::endl;
    }

    std::cout << "Extracted " << extracted << " files to " << outputDir << "/" << std::endl;
    return true;
}

// ============================================================
// CLI helpers
// ============================================================

bool farPackage(const std::string& outputPath, const std::vector<std::string>& inputFiles) {
    FarArchive archive;
    for (const auto& file : inputFiles) {
        std::ifstream in(file, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Cannot read file: " << file << std::endl;
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        // Get basename without extension
        std::string fullName = file;
        size_t sep = fullName.find_last_of("/\\");
        std::string basename = (sep != std::string::npos) ? fullName.substr(sep + 1) : fullName;
        size_t dot = basename.rfind('.');
        std::string stem = (dot != std::string::npos) ? basename.substr(0, dot) : basename;

        std::vector<uint8_t> fcData;

        if (basename.size() > 4 && basename.substr(basename.size() - 4) == ".fox") {
            // Compile .fox -> .fc (each file standalone; dependencies are
            // resolved across .fc entries at runtime)
            try {
                BytecodeCompiler::s_expandImports = false;
                BytecodeCompiler compiler;
                CompiledProgram prog = compiler.compile(content, file);
                fcData = prog.serialize();
                xor_crypt(fcData, FC_XOR_KEY);
                std::cout << "  compiled: " << basename << " -> src/" << stem << ".fc (" << fcData.size() << " bytes)" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "  error compiling " << basename << ": " << e.what() << std::endl;
                return false;
            }
        } else if (basename.size() > 3 && basename.substr(basename.size() - 3) == ".fc") {
            // Use .fc directly
            fcData.assign(content.begin(), content.end());
            std::cout << "  added: src/" << stem << ".fc (" << fcData.size() << " bytes)" << std::endl;
        } else {
            std::cerr << "  unsupported file type: " << basename << " (only .fox and .fc supported)" << std::endl;
            return false;
        }

        archive.addFile("src/" + stem + ".fc", fcData);
    }

    // Add foxconfig/config.txt with version info
    std::string configContent = "version: " + fox_version + "\n";
    std::vector<uint8_t> configData(configContent.begin(), configContent.end());
    archive.addFile("foxconfig/config.txt", configData);

    return archive.save(outputPath);
}

bool farUnpack(const std::string& archivePath, const std::string& outputDir) {
    FarArchive archive;
    if (!archive.load(archivePath)) return false;
    return archive.extractAll(outputDir);
}

bool farRun(const std::string& archivePath) {
    // Load entire archive into memory
    std::ifstream file(archivePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open archive: " << archivePath << std::endl;
        return false;
    }
    std::vector<uint8_t> archiveData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Parse ZIP to find the .fc entry (avoid FarArchive::load which copies data out)
    if (archiveData.size() < 22 || read32le(archiveData.data()) != ZIP_LOCAL_FILE_SIG) {
        std::cerr << "Invalid .far file: not a ZIP archive" << std::endl;
        return false;
    }

    // Find EOCD
    size_t eocd_pos = (archiveData.size() >= 65557) ? archiveData.size() - 65557 : 0;
    bool found = false;
    for (; eocd_pos + 22 <= archiveData.size(); eocd_pos++) {
        if (read32le(archiveData.data() + eocd_pos) == ZIP_EOCD_SIG) {
            found = true;
            break;
        }
    }
    if (!found) {
        std::cerr << "Invalid .far file: no EOCD record" << std::endl;
        return false;
    }

    const uint8_t* eocd = archiveData.data() + eocd_pos;
    uint16_t num_entries = read16le(eocd + 8);
    uint32_t cd_offset   = read32le(eocd + 16);

    // Scan central directory for .fc entries (first entry is the entry point)
    size_t cd_pos = cd_offset;
    struct FCEntry { std::string name; const uint8_t* data; uint32_t size; };
    std::vector<FCEntry> fcEntries;

    for (uint16_t i = 0; i < num_entries; i++) {
        if (cd_pos + 46 > archiveData.size()) break;
        const uint8_t* cd_entry = archiveData.data() + cd_pos;
        if (read32le(cd_entry) != ZIP_CENTRAL_DIR_SIG) break;

        uint16_t name_len = read16le(cd_entry + 28);
        uint16_t extra_len = read16le(cd_entry + 30);
        uint16_t comment_len = read16le(cd_entry + 32);
        uint32_t comp_size = read32le(cd_entry + 20);
        uint32_t local_off = read32le(cd_entry + 42);

        // Bound-check the full central directory entry before reading the name
        if (cd_pos + 46 + (size_t)name_len + extra_len + comment_len > archiveData.size()) {
            std::cerr << "Invalid .far file: central directory entry out of bounds" << std::endl;
            return false;
        }
        std::string name(reinterpret_cast<const char*>(cd_entry + 46), name_len);

        // Check if this is a .fc file
        if (name.size() > 3 && name.substr(name.size() - 3) == ".fc") {
            // Bound-check the local header and file data before reading (P3-3)
            size_t localOff = local_off;
            if (localOff + 30 > archiveData.size()) {
                std::cerr << "Invalid .far file: local header out of bounds" << std::endl;
                return false;
            }
            const uint8_t* local = archiveData.data() + localOff;
            uint16_t lname_len = read16le(local + 26);
            uint16_t lextra_len = read16le(local + 28);
            size_t data_start = localOff + 30 + lname_len + lextra_len;
            if (data_start > archiveData.size() || data_start + comp_size > archiveData.size()) {
                std::cerr << "Invalid .far file: file data out of bounds" << std::endl;
                return false;
            }

            fcEntries.push_back({name, archiveData.data() + data_start, comp_size});
        }

        cd_pos += 46 + name_len + extra_len + comment_len;
    }

    if (fcEntries.empty()) {
        std::cerr << "No .fc file found in archive" << std::endl;
        return false;
    }

    std::cout << "Running: " << fcEntries[0].name << " (" << fcEntries.size()
        << " module(s) in archive)" << std::endl;

    try {
        VM vm;
        bool first = true;
        for (const auto& entry : fcEntries) {
            // Decrypt in-place (archiveData is owned by this function, discarded after)
            std::vector<uint8_t> decrypted(entry.data, entry.data + entry.size);
            xor_crypt(decrypted, FC_XOR_KEY);
            CompiledProgram prog = CompiledProgram::deserialize(decrypted);
            if (first) {
                vm.loadProgram(prog);
                first = false;
            } else {
                vm.addProgram(prog);
            }
        }
        vm.run();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[RuntimeError] " << e.what() << std::endl;
        return false;
    }
}
