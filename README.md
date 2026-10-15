# Interview Preparation

This repository collects coding practice, behavioral interview preparation, system design notes, and company-specific materials.

## Layout

```text
interview/
├── README.md
├── coding/                  # Existing coding repository contents
│   ├── algorithm/
│   ├── leetcode/
│   ├── oa/
│   ├── ood/
│   ├── top_hard/
│   ├── vo/
│   └── interview/           # Remaining original standalone files
├── bq/
│   ├── stories/             # Shared behavioral stories and CARL answers
│   └── deep-dives/          # Detailed project walkthroughs and follow-ups
├── system-design/           # General system design preparation
├── companies/
│   ├── Amazon/
│   └── TikTok/
└── practice/                # Mock interview feedback and improvement notes
```

## Where to Put Materials

- **[Coding](coding/):** Algorithms, implementations, online assessments, and coding interview exercises. The existing internal structure is preserved.
- **[Behavioral preparation](bq/):** General behavioral interview methods and personal preparation. Use `stories/` for concise Context–Actions–Results–Learnings (CARL) answers and `deep-dives/` for project architecture, decisions, tradeoffs, rollout, and detailed follow-ups. These directories currently contain placeholders only.
- **[System design](system-design/README.md):** General concepts, interview frameworks, and design exercises.
- **[Company preparation](companies/):** Company-specific coding, behavioral, domain, and system design materials. Preserve shared facts in the general preparation areas and link to them when adding company-specific emphasis.
- **[Practice](practice/):** Mock interview feedback, gaps to address, and follow-up practice notes. This directory currently contains a placeholder only.

Future behavioral materials can include `bq/guide.md`, `bq/introduction.md`, and `bq/questions-to-ask.md`. They have not been created as part of this structural migration.

## Starting Points

- [Amazon behavioral preparation](companies/Amazon/amazon_bq.md)
- [Amazon coding preparation](companies/Amazon/amazon_coding.md)
- [TikTok behavioral preparation (English)](companies/TikTok/tiktok_bq_en.md)
- [TikTok coding preparation](companies/TikTok/tiktok_coding.md)
- [TikTok system design (English)](companies/TikTok/tiktok_system_design_en.md)
- [General system design index](system-design/README.md)

## Migration Notes

The former `Project/coding` repository is now rooted at `Project/interview`, with its Git history retained. Run repository-level Git commands from this directory.

| Previous path relative to the old repository | New path |
|---|---|
| `system_design/` | `system-design/` |
| `interview/Amazon/` | `companies/Amazon/` |
| `interview/TikTok/` | `companies/TikTok/` |
| All other working-tree contents | `coding/`, with internal paths unchanged |

The original `.gitignore` remains under `coding/`; a copy at the repository root applies the existing ignore rules across the new layout. Existing file contents were preserved, including uncommitted work. Standalone files in the original `coding/interview/` directory were not reassigned to a company.

If an editor tab or external shortcut still points to the former `Project/coding` location, reopen it using the paths above.
