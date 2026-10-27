DWORD WINAPI LookupThreadProc(void*) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    HANDLE events[] = {g_stopEvent, g_lookupJobEvent};
    while (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
        LookupJob job;
        { std::lock_guard lock(g_lookupJobMutex); job = std::move(g_lookupJob); }
        // Coalesce typing on this worker; the render thread never waits for the network.
        while (job.suggestions) {
            const auto event = WaitForMultipleObjects(2, events, FALSE, 250);
            if (event != WAIT_OBJECT_0 + 1) break;
            std::lock_guard lock(g_lookupJobMutex);
            job = std::move(g_lookupJob);
        }
        if (WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) break;
        if (!job.id || job.id != g_lookupRequestSeq.load()) continue;
        try {
            if (job.suggestions) {
                std::vector<LookupRecent> entries;
                if (QuickLookup::LookupSuggestions(job.query, &entries) != QuickLookup::Outcome::Failed)
                    QuickLookup::ApplySuggestions(job.id, std::move(entries));
                continue;
            }
            QuickLookup::Result result;
            const auto outcome = QuickLookup::LookupText(job.query, &result);
            if (WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) break;
            if (job.id != g_lookupRequestSeq.load()) continue;
            if (outcome == QuickLookup::Outcome::NotFound) {
                std::vector<LookupRecent> entries;
                if (QuickLookup::LookupSuggestions(job.query, &entries) == QuickLookup::Outcome::Found) {
                    QuickLookup::ApplySuggestions(job.id, std::move(entries), true);
                    continue;
                }
            }
            if (outcome != QuickLookup::Outcome::Failed)
                QuickLookup::CacheStore(job.key, outcome == QuickLookup::Outcome::Found, result);
            QuickLookup::ApplyResult(job.id, job.query, outcome == QuickLookup::Outcome::Found, result);
            if (outcome == QuickLookup::Outcome::Failed) {
                std::lock_guard lock(g_stateMutex);
                if (job.id == g_lookupRequestSeq.load() && g_state.lookup.active)
                    g_state.lookup.status = LookupStatus::Failed;
            }
        } catch (...) {
            if (job.suggestions) continue;
            std::lock_guard lock(g_stateMutex);
            if (job.id == g_lookupRequestSeq.load() && g_state.lookup.active)
                g_state.lookup.status = LookupStatus::Failed;
            g_layoutDirty = true;
            Wh_Log(L"Quick Lookup: request failed.");
        }
    }
    winrt::uninit_apartment();
    return 0;
}
