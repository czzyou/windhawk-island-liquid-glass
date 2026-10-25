constexpr uint64_t kFileTrayDeleteDelay = 7ULL * 24 * 60 * 60 * 10000000;
constexpr size_t kFileTrayDeleteCapacity = 128;
constexpr wchar_t kFileTrayDeleteKey[] = L"FileTrayPendingDeletesV1";

struct FileTrayDeleteEntry {
    FILETIME requestedAt{};
    BY_HANDLE_FILE_INFORMATION identity{};
    wchar_t path[MAX_PATH]{};
};
static_assert(sizeof(FileTrayDeleteEntry) == 580);

uint64_t FileTrayFileTime(FILETIME time) {
    return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

uint64_t FileTrayDeleteNow() {
    FILETIME time{};
    GetSystemTimeAsFileTime(&time);
    return FileTrayFileTime(time);
}

bool FileTrayDeleteDue(const FileTrayDeleteEntry& entry, uint64_t now) {
    const auto requested = FileTrayFileTime(entry.requestedAt);
    return requested != 0 && now >= requested && now - requested >= kFileTrayDeleteDelay;
}

bool FileTraySameFile(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b) {
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
        a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow &&
        FileTrayFileTime(a.ftCreationTime) == FileTrayFileTime(b.ftCreationTime) &&
        FileTrayFileTime(a.ftLastWriteTime) == FileTrayFileTime(b.ftLastWriteTime) &&
        a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow;
}

bool FileTrayOrdinaryFile(HANDLE file, BY_HANDLE_FILE_INFORMATION& info) {
    return GetFileType(file) == FILE_TYPE_DISK && GetFileInformationByHandle(file, &info) &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
        (info.nFileIndexHigh != 0 || info.nFileIndexLow != 0);
}

// false means retry later; true also covers an absent, replaced or modified file.
bool DeleteExpiredFileTrayItem(const FileTrayDeleteEntry& entry, uint64_t now) {
    if (!FileTrayDeleteDue(entry, now)) return false;
    HANDLE file = CreateFileW(entry.path, DELETE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    BY_HANDLE_FILE_INFORMATION info{};
    bool finished = false;
    if (FileTrayOrdinaryFile(file, info)) {
        if (!FileTraySameFile(entry.identity, info)) finished = true;
        else {
            FILE_DISPOSITION_INFO disposition{TRUE};
            finished = SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition)) != FALSE;
        }
    } else if (GetFileInformationByHandle(file, &info)) {
        finished = true; // A directory or reparse point replaced the original.
    }
    CloseHandle(file);
    return finished;
}

std::vector<FileTrayDeleteEntry> g_fileTrayPendingDeletes;
bool g_fileTrayDeleteStoreHealthy = true;

bool SaveFileTrayDeletes(const std::vector<FileTrayDeleteEntry>& entries) {
    if (entries.size() > kFileTrayDeleteCapacity) return false;
    const DWORD header[] = {1, static_cast<DWORD>(entries.size())};
    std::vector<BYTE> bytes(sizeof(header) + entries.size() * sizeof(FileTrayDeleteEntry));
    memcpy(bytes.data(), header, sizeof(header));
    if (!entries.empty()) memcpy(bytes.data() + sizeof(header), entries.data(), entries.size() * sizeof(FileTrayDeleteEntry));
    return Wh_SetBinaryValue(kFileTrayDeleteKey, bytes.data(), bytes.size()) != FALSE;
}

void LoadFileTrayDeletes() {
    g_fileTrayPendingDeletes.clear();
    g_fileTrayDeleteStoreHealthy = false;
    std::vector<BYTE> bytes(8 + kFileTrayDeleteCapacity * sizeof(FileTrayDeleteEntry));
    const size_t size = Wh_GetBinaryValue(kFileTrayDeleteKey, bytes.data(), bytes.size());
    if (!size) { g_fileTrayDeleteStoreHealthy = true; return; }
    DWORD header[2]{};
    if (size < sizeof(header)) return;
    memcpy(header, bytes.data(), sizeof(header));
    if (header[0] != 1 || header[1] > kFileTrayDeleteCapacity ||
        size != sizeof(header) + header[1] * sizeof(FileTrayDeleteEntry)) return;
    std::vector<FileTrayDeleteEntry> entries(header[1]);
    if (!entries.empty()) memcpy(entries.data(), bytes.data() + sizeof(header), size - sizeof(header));
    for (const auto& entry : entries) {
        const auto end = std::find(std::begin(entry.path), std::end(entry.path), L'\0');
        if (end == std::end(entry.path) || end - entry.path < 3 ||
            !FileTrayFileTime(entry.requestedAt) ||
            !(entry.path[1] == L':' && entry.path[2] == L'\\') ||
            entry.identity.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT) ||
            !(entry.identity.nFileIndexHigh || entry.identity.nFileIndexLow)) return;
    }
    g_fileTrayPendingDeletes = std::move(entries);
    g_fileTrayDeleteStoreHealthy = true;
}

bool ScheduleFileTrayDelete(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    // Only ordinary files are scheduled; removing a folder entry never recursively deletes it.
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) return true;
    if (!g_fileTrayDeleteStoreHealthy || path.size() < 3 || path.size() >= MAX_PATH ||
        path[1] != L':' || path[2] != L'\\') return false;
    HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    FileTrayDeleteEntry entry{};
    const bool ordinary = FileTrayOrdinaryFile(file, entry.identity);
    CloseHandle(file);
    if (!ordinary) return false;
    GetSystemTimeAsFileTime(&entry.requestedAt);
    wcscpy_s(entry.path, path.c_str());
    auto next = g_fileTrayPendingDeletes;
    std::erase_if(next, [&](const auto& item) { return _wcsicmp(item.path, path.c_str()) == 0; });
    next.push_back(entry);
    if (!SaveFileTrayDeletes(next)) return false;
    g_fileTrayPendingDeletes = std::move(next);
    return true;
}

bool CancelFileTrayDelete(const std::wstring& path) {
    if (!g_fileTrayDeleteStoreHealthy) return false;
    auto next = g_fileTrayPendingDeletes;
    std::erase_if(next, [&](const auto& item) { return _wcsicmp(item.path, path.c_str()) == 0; });
    if (next.size() == g_fileTrayPendingDeletes.size()) return true;
    if (!SaveFileTrayDeletes(next)) return false;
    g_fileTrayPendingDeletes = std::move(next);
    return true;
}

void ProcessFileTrayDeletes() {
    const uint64_t now = FileTrayDeleteNow();
    auto next = g_fileTrayPendingDeletes;
    std::erase_if(next, [&](const auto& entry) { return DeleteExpiredFileTrayItem(entry, now); });
    if (next.size() != g_fileTrayPendingDeletes.size() && SaveFileTrayDeletes(next))
        g_fileTrayPendingDeletes = std::move(next);
}
