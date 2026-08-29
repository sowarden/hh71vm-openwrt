# Release notes

Every build copies `release-notes.json` into its signed `release.json`. LuCI only displays a
changelog after the corresponding release descriptor passes the firmware's embedded usign key.
GitHub supplies publication timestamps, which the updater validates and sorts newest first. GitHub
is not trusted to select an image, order the history, or provide changelog text.

Before publishing a source change, edit the root `release-notes.json` file:

```json
{
  "schema": 1,
  "changes": [
    "Describe one user-visible change in plain English.",
    "Describe another tested change."
  ]
}
```

Use one to twelve short entries. Describe changes introduced by this build, not a cumulative copy
of all earlier notes. Keep claims at their verified evidence level and do not include private data,
local paths, credentials, device identifiers, or unreleased plans. The build rejects empty,
duplicated, multiline, padded, or oversized entries.

The notes require human review. A draft may be prepared from the actual source delta when requested,
but publication must never generate or infer release claims automatically from commit subjects.
Re-running a workflow for the same source revision may reuse the same notes because the source delta
is unchanged. Any later source change must update the notes to describe that new release.
