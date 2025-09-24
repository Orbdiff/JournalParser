# JournalParser
Parses the Windows USN Journal to identify file system changes, displaying events related to file and directory creation, modification, renaming, and deletion.

---

# Features

- Direct reading of the USN Journal from NTFS volumes.
- ImGui interface for interactive table visualization.
- Advanced filtering:
   - Case-insensitive.
   - Multiple conditions separated with ";"
     

```
Name:.exe;Reason:file delete → shows all .exe files that were deleted.
.exe;file delete → shows all .exe files that were deleted.
```