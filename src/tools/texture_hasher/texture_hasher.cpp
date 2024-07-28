//
// RT64
//

#include <filesystem>
#include <fstream>
#include <list>

#include "../../contrib/xxHash/xxh3.h"
#include "../../common/rt64_load_types.cpp"
#include "../../common/rt64_replacement_database.cpp"
#include "../../shared/rt64_f3d_defines.h"
#include "../../common/rt64_tmem_hasher.h"

static const std::string TileInfoExtension = ".tile.json";
static const std::string TMEMExtension = ".tmem";
static const std::string RiceInfoExtension = ".rice.json";
static const std::string RiceRdramExtension = ".rice.rdram";
static const std::string RicePaletteInfoExtension = ".rice.palette.json";
static const std::string RicePaletteRdramExtension = ".rice.palette.rdram";

bool endsWith(const std::string &str, const std::string &end) {
    if (str.length() >= end.length()) {
        return (str.compare(str.length() - end.length(), end.length(), end) == 0);
    }
    else {
        return false;
    }
}

bool loadTileInfoFromFile(const std::filesystem::path &directory, const std::string &hashName, RT64::LoadTile &tile, RT64::LoadTLUT &tlut, uint32_t &width, uint32_t &height) {
    try {
        std::filesystem::path infoPath = directory / (hashName + TileInfoExtension);
        if (!std::filesystem::exists(infoPath)) {
            return false;
        }

        std::ifstream infoStream(infoPath);
        json jroot;
        infoStream >> jroot;
        tile = jroot["tile"];
        tlut = jroot["tlut"];
        width = jroot["width"];
        height = jroot["height"];
        if (!infoStream.bad()) {
            return true;
        }
        else {
            std::string u8string = infoPath.u8string();
            fprintf(stderr, "Failed to read file at %s.", u8string.c_str());
            return false;
        }
    }
    catch (const nlohmann::detail::exception &e) {
        fprintf(stderr, "JSON parsing error: %s\n", e.what());
        return false;
    }
}

bool loadOperationInfoFromFile(const std::filesystem::path &directory, const std::string &hashName, RT64::LoadOperation &loadOp) {
    try {
        std::filesystem::path infoPath = directory / (hashName + RiceInfoExtension);
        if (!std::filesystem::exists(infoPath)) {
            return false;
        }

        std::ifstream infoStream(infoPath);
        json jroot;
        infoStream >> jroot;
        loadOp.texture = jroot["texture"];
        loadOp.tile = jroot["tile"];
        loadOp.type = jroot["type"];
        if (!infoStream.bad()) {
            return true;
        }
        else {
            std::string u8string = infoPath.u8string();
            fprintf(stderr, "Failed to read file at %s.", u8string.c_str());
            return false;
        }
    }
    catch (const nlohmann::detail::exception &e) {
        fprintf(stderr, "JSON parsing error: %s\n", e.what());
        return false;
    }
}

bool loadBytesFromFile(const std::filesystem::path &path, std::vector<uint8_t> &bytes) {
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        bytes.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read((char *)(bytes.data()), bytes.size());
        return !file.bad();
    }
    else {
        return false;
    }
}

uint32_t toHashTLUT(RT64::LoadTLUT drawTLUT) {
    switch (drawTLUT) {
    case RT64::LoadTLUT::RGBA16:
        return G_TT_RGBA16;
    case RT64::LoadTLUT::IA16:
        return G_TT_IA16;
    default:
        return 0;
    }
}

struct RenameFile {
    const std::filesystem::path oldPath;
    const std::filesystem::path newPath;
};

void queueRenameFile(const std::filesystem::path &directory, const std::string &oldHashName, const std::string &newHashName, const std::string &suffix, std::list<RenameFile> &renameFileList) {
    // Skip if the file with the old name doesn't exist.
    const std::filesystem::path oldPath = directory / (oldHashName + suffix);
    if (!std::filesystem::exists(oldPath)) {
        return;
    }

    // Skip if the file with the new name already exists.
    const std::filesystem::path newPath = directory / (newHashName + suffix);
    if (std::filesystem::exists(newPath)) {
        return;
    }

    renameFileList.push_back({ oldPath, newPath });
}

void queueRenameFiles(const std::filesystem::path &directory, const std::string &oldHashName, const std::string &newHashName, std::list<RenameFile> &renameFileList) {
    if (oldHashName == newHashName) {
        return;
    }

    queueRenameFile(directory, oldHashName, newHashName, TileInfoExtension, renameFileList);
    queueRenameFile(directory, oldHashName, newHashName, TMEMExtension, renameFileList);
    queueRenameFile(directory, oldHashName, newHashName, RiceInfoExtension, renameFileList);
    queueRenameFile(directory, oldHashName, newHashName, RiceRdramExtension, renameFileList);
    queueRenameFile(directory, oldHashName, newHashName, RicePaletteInfoExtension, renameFileList);
    queueRenameFile(directory, oldHashName, newHashName, RicePaletteRdramExtension, renameFileList);
}

void executeRenameFiles(const std::list<RenameFile> &renameFileList) {
    for (const RenameFile &renameFile : renameFileList) {
        std::error_code ec;
        std::filesystem::rename(renameFile.oldPath, renameFile.newPath, ec);
        if (ec) {
            const std::string oldPathStr = renameFile.oldPath.u8string();
            const std::string newPathStr = renameFile.newPath.u8string();
            const std::string messageStr = ec.message();
            fprintf(stderr, "Failed to rename %s to %s (%s).\n", oldPathStr.c_str(), newPathStr.c_str(), messageStr.c_str());
        }
    }
}

void addRiceHash(const std::filesystem::path &directory, const std::string &hashNameWithSuffix, RT64::ReplacementDatabase &database, std::list<RenameFile> &renameFileList) {
    RT64::LoadTile drawTile = {};
    RT64::LoadTLUT drawTLUT;
    uint32_t drawWidth, drawHeight;
    if (!loadTileInfoFromFile(directory, hashNameWithSuffix, drawTile, drawTLUT, drawWidth, drawHeight)) {
        return;
    }

    RT64::LoadOperation loadOp = {};
    if (!loadOperationInfoFromFile(directory, hashNameWithSuffix, loadOp)) {
        return;
    }

    std::vector<uint8_t> rdramBytes;
    if (!loadBytesFromFile(directory / (hashNameWithSuffix + RiceRdramExtension), rdramBytes)) {
        return;
    }

    // The code below has been referenced from GlideN64 and RiceVideo to imitate their hashing
    // algorithms. To comply with their licenses, the texture hasher tool is also under the GPL.
    auto CalculateDXT = [&](uint32_t txl2words) {
        if (txl2words == 0) {
            return 1U;
        }
        else {
            return (2048 + txl2words - 1) / txl2words;
        }
    };

    auto Txl2Words = [&](uint32_t width, uint32_t size) {
        static uint32_t sizeBytes[4] = { 0, 1, 2, 4 };
        if (size == 0) {
            return std::max(1U, width / 16);
        }
        else {
            return std::max(1U, width * sizeBytes[size] / 8);
        }
    };

    auto ReverseDXT = [&](uint32_t val, uint32_t width, uint32_t size) {
        if (val == 0x800) {
            return 1;
        }

        int low = 2047 / val;
        if (CalculateDXT(low) > val) {
            low++;
        }

        int high = 2047 / (val - 1);
        if (low == high) {
            return low;
        }

        for (int i = low; i <= high; i++) {
            if (Txl2Words(width, size) == (uint32_t)i) {
                return i;
            }
        }

        return (low + high) / 2;
    };

    int bpl;
    int width, height;
    if (loadOp.type == RT64::LoadOperation::Type::Tile) {
        uint16_t tileWidth = (std::max((loadOp.tile.lrs >> 2) - (loadOp.tile.uls >> 2), 0) + 1) & 0x03FF;
        uint16_t tileHeight = (std::max((loadOp.tile.lrt >> 2) - (loadOp.tile.ult >> 2), 0) + 1) & 0x03FF;
        tileWidth = (loadOp.tile.masks != 0) ? std::min(tileWidth, uint16_t(1U << loadOp.tile.masks)) : tileWidth;
        bpl = loadOp.texture.width << loadOp.texture.siz >> 1;
        width = std::min(tileWidth, loadOp.texture.width);
        if (loadOp.tile.siz > drawTile.siz) {
            width <<= loadOp.tile.siz - drawTile.siz;
        }

        height = (loadOp.tile.maskt != 0) ? std::min(tileHeight, uint16_t(1U << loadOp.tile.maskt)) : tileHeight;
    }
    else if (loadOp.type == RT64::LoadOperation::Type::Block) {
        int tile_width = std::max((drawTile.lrs >> 2) - (drawTile.uls >> 2), 0) + 1;
        int tile_height = std::max((drawTile.lrt >> 2) - (drawTile.ult >> 2), 0) + 1;
        int mask_width = (drawTile.masks == 0) ? (tile_width) : (1 << drawTile.masks);
        int mask_height = (drawTile.maskt == 0) ? (tile_height) : (1 << drawTile.maskt);
        bool clamps = (drawTile.masks == 0) || (drawTile.cms & G_TX_CLAMP);
        bool clampt = (drawTile.maskt == 0) || (drawTile.cmt & G_TX_CLAMP);
        if (clamps && (tile_width <= 256)) {
            width = std::min(mask_width, tile_width);
        }
        else {
            width = mask_width;
        }

        if ((clampt && (tile_height <= 256)) || (mask_height > 256)) {
            height = std::min(mask_height, tile_height);
        }
        else {
            height = mask_height;
        }

        if (drawTile.siz == G_IM_SIZ_32b)
            bpl = drawTile.line << 4;
        else if (loadOp.tile.lrt == 0) {
            bpl = drawTile.line << 3;
        }
        else {
            uint32_t dxt = loadOp.tile.lrt;
            if (dxt > 1) {
                dxt = ReverseDXT(dxt, loadOp.texture.width, loadOp.texture.siz);
            }

            bpl = dxt << 3;
        }
    }
    else {
        // Invalid case.
        return;
    }

    auto CalculateMaxCI8b = [&](const uint8_t *src, uint32_t width, uint32_t height, uint32_t rowStride) {
        uint8_t val = 0;
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t *buf = src + rowStride * y;
            for (uint32_t x = 0; x < width; ++x) {
                if (buf[x] > val) {
                    val = buf[x];
                }

                if (val == 0xFF) {
                    return uint8_t(0xFF);
                }
            }
        }

        return val;
    };

    auto CalculateMaxCI4b = [&](const uint8_t *src, uint32_t width, uint32_t height, uint32_t rowStride) {
        uint8_t val = 0;
        uint8_t val1, val2;
        width >>= 1;
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t *buf = src + rowStride * y;
            for (uint32_t x = 0; x < width; ++x) {
                val1 = buf[x] >> 4;
                val2 = buf[x] & 0xF;
                if (val1 > val) {
                    val = val1;
                }

                if (val2 > val) {
                    val = val2;
                }

                if (val == 0xF) {
                    return uint8_t(0xF);
                }
            }
        }

        return val;
    };

    auto RiceCRC32 = [](const uint8_t *src, int width, int height, int size, int rowStride) {
        uint32_t crc32Ret = 0;
        const uint32_t bytesPerLine = width << size >> 1;
        int y = height - 1;
        while (y >= 0) {
            uint32_t esi = 0;
            int x = bytesPerLine - 4;
            while (x >= 0) {
                esi = *(uint32_t *)(src + x);
                esi ^= x;

                crc32Ret = (crc32Ret << 4) + ((crc32Ret >> 28) & 15);
                crc32Ret += esi;
                x -= 4;
            }

            esi ^= y;
            crc32Ret += esi;
            src += rowStride;
            --y;
        }

        return crc32Ret;
    };
    
    // Rice expects to hash at least this many bytes, even if it leaks into other parts of RAM.
    uint32_t expectedSize = (height - 1) * bpl + (width << drawTile.siz >> 1);
    if (expectedSize > rdramBytes.size()) {
        fprintf(stderr, "Unable to hash %s. Expected %d bytes but got %zu bytes (%dX%d).\n", hashNameWithSuffix.c_str(), expectedSize, rdramBytes.size(), width, height);
        return;
    }

    // Extract the version from the hash name with the suffix.
    const std::string dotV = ".v";
    const size_t vPos = hashNameWithSuffix.find_first_of(dotV);
    bool redoHash = false;
    if (vPos != std::string::npos) {
        try {
            int versionFromName = std::stoi(hashNameWithSuffix.substr(vPos + dotV.size()));
            if (versionFromName != RT64::TMEMHasher::CurrentHashVersion) {
                redoHash = true;
            }
        }
        catch (const std::exception &e) {
            redoHash = true;
        }
    }
    else {
        redoHash = true;
    }

    // Redo the hash if necessary by loading from TMEM. If it requires raw TMEM, the hash is already correct.
    std::string currentHashName;
    if (redoHash && !RT64::TMEMHasher::requiresRawTMEM(drawTile, drawWidth, drawHeight)) {
        std::vector<uint8_t> tmemBytes;
        if (!loadBytesFromFile(directory / (hashNameWithSuffix + TMEMExtension), tmemBytes)) {
            return;
        }

        const uint64_t hash = RT64::TMEMHasher::hash(tmemBytes.data(), drawTile, drawWidth, drawHeight, toHashTLUT(drawTLUT), RT64::TMEMHasher::CurrentHashVersion);
        currentHashName = RT64::ReplacementDatabase::hashToString(hash);
    }
    else {
        currentHashName = hashNameWithSuffix.substr(0, vPos);
    }

    uint32_t riceCrc = RiceCRC32(rdramBytes.data(), width, height, drawTile.siz, bpl);
    RT64::ReplacementTexture replacement = database.getReplacement(currentHashName);
    replacement.hashes.rt64 = currentHashName;
    replacement.hashes.rice = RT64::ReplacementDatabase::hashToString(riceCrc) + "#" + std::to_string(drawTile.fmt) + "#" + std::to_string(drawTile.siz);

    // Add the palette hash if necessary.
    std::vector<uint8_t> paletteBytes;
    if (drawTLUT != RT64::LoadTLUT::None) {
        if (!loadBytesFromFile(directory / (hashNameWithSuffix + RicePaletteRdramExtension), paletteBytes)) {
            return;
        }

        uint8_t cimax = 0;
        uint32_t ricePaletteCrc = 0;
        if (drawTile.siz == G_IM_SIZ_4b) {
            cimax = CalculateMaxCI4b(rdramBytes.data(), width, height, bpl);
            ricePaletteCrc = RiceCRC32(paletteBytes.data(), cimax + 1, 1, 2, 32);
        }
        else {
            cimax = CalculateMaxCI8b(rdramBytes.data(), width, height, bpl);
            ricePaletteCrc = RiceCRC32(paletteBytes.data(), cimax + 1, 1, 2, 512);
        }
        
        replacement.hashes.rice += "#" + RT64::ReplacementDatabase::hashToString(ricePaletteCrc);
    }

    database.addReplacement(replacement);

    if (redoHash) {
        const std::string newVersionSuffix = ".v" + std::to_string(RT64::TMEMHasher::CurrentHashVersion);
        queueRenameFiles(directory, hashNameWithSuffix, currentHashName + newVersionSuffix, renameFileList);
    }
}

void upgradeHash(const std::filesystem::path &directory, const std::string oldHashName, RT64::ReplacementDatabase &database, std::list<RenameFile> &renameFileList) {
    RT64::ReplacementTexture replacement = database.getReplacement(oldHashName);
    if (replacement.isEmpty()) {
        return;
    }
    
    RT64::LoadTile drawTile = {};
    RT64::LoadTLUT drawTLUT;
    uint32_t drawWidth, drawHeight;
    const std::string oldVersionSuffix = (database.config.hashVersion > 1) ? (".v" + std::to_string(database.config.hashVersion)) : "";
    if (!loadTileInfoFromFile(directory, oldHashName + oldVersionSuffix, drawTile, drawTLUT, drawWidth, drawHeight)) {
        return;
    }

    std::vector<uint8_t> tmemBytes;
    if (!loadBytesFromFile(directory / (oldHashName + oldVersionSuffix + TMEMExtension), tmemBytes)) {
        return;
    }
    
    const uint64_t hash = RT64::TMEMHasher::hash(tmemBytes.data(), drawTile, drawWidth, drawHeight, toHashTLUT(drawTLUT), RT64::TMEMHasher::CurrentHashVersion);
    const std::string newHashName = RT64::ReplacementDatabase::hashToString(hash);
    if (oldHashName != newHashName) {
        database.fixReplacement(oldHashName, replacement);
        fprintf(stdout, "Updated %s to %s in database.\n", oldHashName.c_str(), newHashName.c_str());

        const std::string newVersionSuffix = ".v" + std::to_string(RT64::TMEMHasher::CurrentHashVersion);
        queueRenameFiles(directory, oldHashName + oldVersionSuffix, newHashName + newVersionSuffix, renameFileList);
    }
}

void showHelp() {
    fprintf(stderr, 
        "texture_hasher <path> --rice\n"
        "\tGenerate Rice matches based on the dumped textures.\n\n"
        "texture_hasher <path> --upgrade\n"
        "\tUpgrade the database to the latest version. Will recalculate any hashes as necessary.\n\n"
    );
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        showHelp();
        return 1;
    }

    std::filesystem::path searchDirectory(argv[1]);
    if (!std::filesystem::is_directory(searchDirectory)) {
        std::string u8string = searchDirectory.u8string();
        fprintf(stderr, "The directory %s does not exist.", u8string.c_str());
        return 1;
    }

    enum class Mode {
        Rice,
        Upgrade
    };

    Mode mode = Mode::Rice;
    if (argc > 2) {
        std::string modeString = argv[2];
        if ((modeString == "--upgrade") || (modeString == "-u")) {
            fprintf(stdout, "Upgrading database to latest hash version.\n");
            mode = Mode::Upgrade;
        }
        else if ((modeString == "--rice") || (modeString == "-r")) {
            fprintf(stdout, "Generating Rice hashes for files in the directory.\n");
            mode = Mode::Rice;
        }
        else {
            fprintf(stderr, "Unrecognized argument %s.\n\n", modeString.c_str());
            showHelp();
            return 1;
        }
    }

    RT64::ReplacementDatabase database;
    database.config.autoPath = RT64::ReplacementAutoPath::Rice;

    std::filesystem::path databasePath = searchDirectory / "rt64.json";
    if (std::filesystem::exists(databasePath)) {
        std::ifstream databaseStream(databasePath);
        if (databaseStream.is_open()) {
            try {
                json jroot;
                databaseStream >> jroot;
                database = jroot;
                if (databaseStream.bad()) {
                    std::string u8string = databasePath.u8string();
                    fprintf(stderr, "Failed to read database file at %s.", u8string.c_str());
                    return 1;
                }
            }
            catch (const nlohmann::detail::exception &e) {
                fprintf(stderr, "JSON parsing error: %s\n", e.what());
                return 1;
            }
        }
    }
    else if (mode == Mode::Upgrade) {
        fprintf(stderr, "Database file rt64.json is missing.\n");
        return 1;
    }

    std::list<RenameFile> renameFileList;
    if (mode == Mode::Rice) {
        if (database.config.hashVersion < RT64::TMEMHasher::CurrentHashVersion) {
            fprintf(stderr, "Database hash version (%u) is older than texture hasher's version (%u). Upgrade it first using the --upgrade command.\n", database.config.hashVersion, RT64::TMEMHasher::CurrentHashVersion);
            return 1;
        }

        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(searchDirectory)) {
            if (entry.is_regular_file()) {
                const std::string filename = entry.path().filename().u8string();
                if (endsWith(filename, RiceInfoExtension)) {
                    addRiceHash(searchDirectory, filename.substr(0, filename.size() - RiceInfoExtension.size()), database, renameFileList);
                }
            }
        }
    }
    else if (mode == Mode::Upgrade) {
        if (database.config.hashVersion > RT64::TMEMHasher::CurrentHashVersion) {
            fprintf(stderr, "Database hash version (%u) is newer than texture hasher's version (%u).\n", database.config.hashVersion, RT64::TMEMHasher::CurrentHashVersion);
            return 1;
        }
        else if (database.config.hashVersion == RT64::TMEMHasher::CurrentHashVersion) {
            fprintf(stderr, "Database hash version is already up to date.\n");
            return 1;
        }

        for (RT64::ReplacementTexture &texture : database.textures) {
            upgradeHash(searchDirectory, texture.hashes.rt64, database, renameFileList);
        }

        database.config.hashVersion = RT64::TMEMHasher::CurrentHashVersion;
    }

    // Save new database file.
    std::filesystem::path databaseNewPath = databasePath.u8string() + ".new";
    std::ofstream databaseNewStream(databaseNewPath);
    if (!databaseNewStream.is_open()) {
        std::string u8string = databaseNewPath.u8string();
        fprintf(stderr, "Failed to open database file at %s for writing.", u8string.c_str());
        return 1;
    }

    try {
        json jroot = database;
        databaseNewStream << std::setw(4) << jroot << std::endl;
        if (!databaseNewStream.bad()) {
            databaseNewStream.close();
        }
        else {
            std::string u8string = databaseNewPath.u8string();
            fprintf(stderr, "Failed to write to database file at %s.", u8string.c_str());
            return 1;
        }
    }
    catch (const nlohmann::detail::exception &e) {
        fprintf(stderr, "JSON writing error: %s\n", e.what());
        return 1;
    }

    // If everything went OK, swap the names and delete the old file.
    std::error_code ec;
    std::filesystem::path databaseOldPath = databasePath.u8string() + ".old";
    if (std::filesystem::exists(databasePath)) {
        if (std::filesystem::exists(databaseOldPath)) {
            std::filesystem::remove(databaseOldPath, ec);
            if (ec) {
                std::string u8string = databaseOldPath.u8string();
                fprintf(stderr, "Failed to remove old database file at %s.", u8string.c_str());
                return 1;
            }
        }

        std::filesystem::rename(databasePath, databaseOldPath, ec);
        if (ec) {
            std::string u8stringOld = databaseOldPath.u8string();
            std::string u8stringNew = databaseNewPath.u8string();
            fprintf(stderr, "Failed to rename old database file from %s to %s.\n", u8stringOld.c_str(), u8stringNew.c_str());
            return 1;
        }
    }

    std::filesystem::rename(databaseNewPath, databasePath, ec);
    if (ec) {
        std::string u8stringNew = databaseNewPath.u8string();
        std::string u8stringCur = databasePath.u8string();
        fprintf(stderr, "Failed to rename new database file from %s to %s.\n", u8stringNew.c_str(), u8stringCur.c_str());
        return 1;
    }

    // Once the database has been upgraded successfully, rename all the files that were detected.
    if (!renameFileList.empty()) {
        executeRenameFiles(renameFileList);
    }
    
    return 0;
}
