#ifndef ESPALTHERMA_CATALOG_H
#define ESPALTHERMA_CATALOG_H

// Label catalog of the generic firmware: the values of every model, merged from include/def/*.h
// by scripts/gen_catalog.py into include/catalog_data.h. Each entry carries the mask of the models
// (definition files) that have it.

#include <stdint.h>
#include <string.h>
#include <vector>
#include "labeldef.h"

struct CatalogModel
{
  const char *name; // definition file name, without .h
  char family;      // 'G' Gen-2, 'L' Gen-1, 'S' Protocol S, 'R' Protocol S (ROTEX)
  char protocol;    // 'I' or 'S'
  uint8_t capMin;   // capacity range in kW, from the definition name (0: not told)
  uint8_t capMax;
};

struct CatalogEntry
{
  uint8_t reg;
  uint8_t offset;
  uint16_t conv;
  uint8_t size;
  int8_t type;
  uint16_t label; // offset in CATALOG_LABELS
  uint8_t flags;
  uint64_t models; // bit i: CATALOG_MODELS[i] has this entry
};

struct CatalogFingerprint
{
  const char *key; // prefix of a survey key (survey.h surveyKey)
  int16_t model;
};

struct CatalogAsNumber
{
  uint32_t body;   // 7 digits of the indoor unit AS number, eg. 1708171 for AS1708171-30
  uint8_t suffix1; // first suffix digit, 0xFF: any
  uint8_t suffix2; // second suffix digit, 0xFF: any
  uint64_t models; // candidate models (definition files)
};

// Sensor names in another language (gen_catalog.py, from include/def/<Language>/)
struct CatalogTranslation
{
  uint16_t label; // offset of the English name in CATALOG_LABELS
  uint16_t text;  // offset of the translation in the language pool
};

struct CatalogLanguage
{
  const char *code; // "fr", "de"...
  const char *pool;
  const CatalogTranslation *items; // sorted by label
  uint16_t count;
};

#define CATALOG_FLAG_RECOMMENDED 1 // enabled by default
#define CATALOG_FLAG_ALWAYS 2      // refrigerant type: no data, selects the pressure->temperature conversion

#include "catalog_data.h"

#define CATALOG_MAX_SELECTED 128

// A registry read with the entries of another model (layoutfix.h)
struct CatalogFix
{
  uint8_t reg;
  int model;
};

// The model whose entries a registry is read with
inline int catalogModelFor(uint8_t reg, int model, const CatalogFix *fixes, size_t fixCount)
{
  for (size_t f = 0; f < fixCount; f++)
  {
    if (fixes[f].reg == reg)
      return fixes[f].model;
  }
  return model;
}

inline const char *catalogLabel(const CatalogEntry &e)
{
  return CATALOG_LABELS + e.label;
}

// Index of a language in CATALOG_LANGUAGES, -1 for English or an unknown code.
inline int catalogLanguage(const char *code)
{
  for (int i = 0; code != nullptr && i < CATALOG_LANGUAGE_COUNT; i++)
  {
    if (strcmp(CATALOG_LANGUAGES[i].code, code) == 0)
      return i;
  }
  return -1;
}

// Name of an entry in a language (catalogLanguage()), English when it has no translation.
inline const char *catalogName(const CatalogEntry &e, int language)
{
  if (language < 0 || language >= CATALOG_LANGUAGE_COUNT)
    return catalogLabel(e);
  const CatalogLanguage &l = CATALOG_LANGUAGES[language];
  int low = 0, high = (int)l.count - 1;
  while (low <= high)
  {
    int mid = (low + high) / 2;
    if (l.items[mid].label == e.label)
      return l.pool + l.items[mid].text;
    if (l.items[mid].label < e.label)
      low = mid + 1;
    else
      high = mid - 1;
  }
  return catalogLabel(e);
}

inline bool catalogInModel(const CatalogEntry &e, int model)
{
  return model >= 0 && ((e.models >> model) & 1);
}

// Stable identifier of a value (registry, offset, conversion, size), used to persist the selection:
// it survives a regeneration of the catalog.
inline uint32_t catalogKey(const CatalogEntry &e)
{
  return (uint32_t)e.reg << 24 | (uint32_t)e.offset << 16 | (uint32_t)(e.conv & 0x3FF) << 4 | (e.size & 0xF);
}

// Index of the model with this name, -1 if unknown.
int catalogFindModel(const char *name)
{
  if (name == nullptr || name[0] == 0)
    return -1;
  for (int i = 0; i < CATALOG_MODEL_COUNT; i++)
  {
    if (strcmp(CATALOG_MODELS[i].name, name) == 0)
      return i;
  }
  return -1;
}

static bool catalogKeySelected(uint32_t key, const uint32_t *keys, size_t keyCount)
{
  for (size_t i = 0; i < keyCount; i++)
  {
    if (keys[i] == key)
      return true;
  }
  return false;
}

// Builds the labels to query for a model: the selected keys, or its recommended values when nothing is selected.
// Returns the refrigerant conversion of the model (801 R410A, 802 R32, 803 R22), or 0 if it does not tell.
// Registries in fixes are read with the entries of their fix model. The display names are in language
// (catalogLanguage()); the labels, used as MQTT keys and Home Assistant ids, stay English.
int catalogBuildLabels(int model, const uint32_t *keys, size_t keyCount, std::vector<LabelDef> &out,
                       const CatalogFix *fixes = nullptr, size_t fixCount = 0, int language = -1)
{
  out.clear();
  int refrigerant = 0;
  for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
  {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (e.flags & CATALOG_FLAG_ALWAYS)
    {
      if (catalogInModel(e, model))
        refrigerant = e.conv;
      continue;
    }
    if (!catalogInModel(e, catalogModelFor(e.reg, model, fixes, fixCount)))
      continue;
    bool selected = keyCount == 0 ? (e.flags & CATALOG_FLAG_RECOMMENDED) != 0 : catalogKeySelected(catalogKey(e), keys, keyCount);
    if (!selected || out.size() >= CATALOG_MAX_SELECTED)
      continue;
    out.push_back(LabelDef(e.reg, e.offset, e.conv, e.size, e.type, catalogLabel(e)));
    memset(out.back().asString, 0, sizeof(out.back().asString));
    out.back().data = nullptr;
    out.back().name = catalogName(e, language);
  }
  return refrigerant;
}

#endif
