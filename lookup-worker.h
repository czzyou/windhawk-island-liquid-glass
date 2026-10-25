DWORD WINAPI LookupThreadProc(void*) {
    HANDLE events[] = {g_stopEvent, g_lookupJobEvent};
    while (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
        LookupJob job;
        { std::lock_guard lock(g_lookupJobMutex); job = std::move(g_lookupJob); }
        if (!job.id || job.id != g_lookupRequestSeq.load()) continue;
        try {
            QuickLookup::Result result;
            const auto outcome = QuickLookup::LookupText(job.query, &result);
            if (WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) break;
            if (outcome != QuickLookup::Outcome::Failed)
                QuickLookup::CacheStore(job.key, outcome == QuickLookup::Outcome::Found, result);
            QuickLookup::ApplyResult(job.id, job.query, outcome == QuickLookup::Outcome::Found, result);
            if (outcome == QuickLookup::Outcome::Failed) {
                std::lock_guard lock(g_stateMutex);
                if (job.id == g_lookupRequestSeq.load() && g_state.lookup.active)
                    g_state.lookup.status = LookupStatus::Failed;
            }
        } catch (...) {
            std::lock_guard lock(g_stateMutex);
            if (job.id == g_lookupRequestSeq.load() && g_state.lookup.active)
                g_state.lookup.status = LookupStatus::Failed;
            g_layoutDirty = true;
            Wh_Log(L"Quick Lookup: request failed.");
        }
    }
    return 0;
}
