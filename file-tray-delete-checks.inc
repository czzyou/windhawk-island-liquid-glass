void CheckFileTrayDeletion() {
    wchar_t temp[MAX_PATH]{}, path[MAX_PATH]{};
    assert(GetTempPathW(MAX_PATH, temp));
    assert(GetTempFileNameW(temp, L"ift", 0, path));
    assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    LoadFileTrayDeletes();
    assert(g_fileTrayDeleteStoreHealthy && g_fileTrayPendingDeletes.empty());
    assert(ScheduleFileTrayDelete(path));
    assert(g_fileTrayPendingDeletes.size() == 1);
    const auto original = g_fileTrayPendingDeletes[0];
    const uint64_t requested = FileTrayFileTime(original.requestedAt);
    assert(!FileTrayDeleteDue(original, requested - 1));
    assert(!FileTrayDeleteDue(original, requested + kFileTrayDeleteDelay - 1));
    assert(!DeleteExpiredFileTrayItem(original, requested + kFileTrayDeleteDelay - 1));
    assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    assert(FileTrayDeleteDue(original, requested + kFileTrayDeleteDelay));
    g_fileTrayPendingDeletes.clear();
    LoadFileTrayDeletes();
    assert(g_fileTrayPendingDeletes.size() == 1 && g_fileTrayPendingDeletes[0].path == std::wstring(path));
    fileTrayDeleteWriteFails = true;
    assert(!CancelFileTrayDelete(path) && g_fileTrayPendingDeletes.size() == 1);
    fileTrayDeleteWriteFails = false;
    assert(CancelFileTrayDelete(path) && g_fileTrayPendingDeletes.empty());
    LoadFileTrayDeletes();
    assert(g_fileTrayPendingDeletes.empty());

    assert(ScheduleFileTrayDelete(path));
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    assert(file != INVALID_HANDLE_VALUE);
    DWORD written=0;
    assert(WriteFile(file, "edited", 6, &written, nullptr) && written == 6);
    assert(CloseHandle(file));
    assert(DeleteExpiredFileTrayItem(original, requested + kFileTrayDeleteDelay));
    assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES); // edited file survives
    assert(ScheduleFileTrayDelete(path));
    const auto changed = g_fileTrayPendingDeletes[0];
    file = CreateFileW(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    assert(file != INVALID_HANDLE_VALUE);
    const uint64_t due = FileTrayFileTime(changed.requestedAt) + kFileTrayDeleteDelay;
    assert(!DeleteExpiredFileTrayItem(changed, due)); // occupied file is retried
    assert(CloseHandle(file));
    assert(DeleteExpiredFileTrayItem(changed, due));
    assert(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);

    // A new file at the original path must survive the old deletion record.
    file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    assert(file != INVALID_HANDLE_VALUE);
    assert(WriteFile(file, "replacement", 11, &written, nullptr) && written == 11);
    assert(CloseHandle(file));
    assert(DeleteExpiredFileTrayItem(changed, due));
    assert(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
    assert(DeleteFileW(path)); // only this test's own temporary file

    assert(CancelFileTrayDelete(path));
    assert(CreateDirectoryW(path, nullptr));
    assert(ScheduleFileTrayDelete(path) && g_fileTrayPendingDeletes.empty());
    assert(GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY);
    assert(RemoveDirectoryW(path));

    assert(GetTempFileNameW(temp, L"ift", 0, path));
    assert(ScheduleFileTrayDelete(path));
    const uint64_t aged = FileTrayDeleteNow() - kFileTrayDeleteDelay - 10000000;
    g_fileTrayPendingDeletes[0].requestedAt = {static_cast<DWORD>(aged), static_cast<DWORD>(aged >> 32)};
    assert(SaveFileTrayDeletes(g_fileTrayPendingDeletes));
    g_fileTrayPendingDeletes.clear();
    LoadFileTrayDeletes();
    ProcessFileTrayDeletes();
    assert(g_fileTrayPendingDeletes.empty() && GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES);
    LoadFileTrayDeletes();
    assert(g_fileTrayPendingDeletes.empty());

    fileTrayDeleteWriteFails = true;
    assert(!SaveFileTrayDeletes({}));
    fileTrayDeleteWriteFails = false;
    fileTrayDeleteStorage = {1, 0, 0};
    LoadFileTrayDeletes();
    assert(!g_fileTrayDeleteStoreHealthy && g_fileTrayPendingDeletes.empty());
    assert(!CancelFileTrayDelete(L"C:\\unused"));
    fileTrayDeleteStorage.clear();
    LoadFileTrayDeletes();
}
