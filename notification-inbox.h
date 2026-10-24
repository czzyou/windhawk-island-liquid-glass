struct NotificationInbox {
    std::vector<NotificationSnapshot> items;
    size_t selected = 0;
    uint64_t nextToken = 1;

    void Push(NotificationSnapshot& item, bool preserveSelection) {
        const uint64_t previous = items.empty() ? 0 : items[selected].token;
        auto existing = std::find_if(items.begin(), items.end(), [&](const auto& entry) {
            return item.systemId && entry.systemId == item.systemId && entry.sourceAppId == item.sourceAppId;
        });
        if (existing != items.end()) {
            item.token = existing->token;
            item.receivedTime = existing->receivedTime;
            *existing = item;
            return;
        }
        item.token = nextToken++;
        items.insert(items.begin(), item);
        if (items.size() > 20) items.resize(20);
        selected = 0;
        if (preserveSelection && previous) {
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].token == previous) { selected = i; break; }
        }
    }

    void Move(int direction) {
        if (items.empty()) return;
        if (direction > 0 && selected + 1 < items.size()) ++selected;
        if (direction < 0 && selected > 0) --selected;
    }

    void RemoveSelected() {
        if (items.empty()) return;
        items.erase(items.begin() + selected);
        if (selected >= items.size()) selected = items.empty() ? 0 : items.size() - 1;
    }
};
