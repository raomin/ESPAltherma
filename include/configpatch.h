#ifndef ESPALTHERMA_CONFIGPATCH_H
#define ESPALTHERMA_CONFIGPATCH_H
#include <atomic>
#include <new>
#include <utility>

enum class ConfigQueueResult {queued,busy,no_memory};
// Request task transfers ownership; main loop keeps the slot busy until the patch has been applied.
class ConfigPatchQueue
{
  std::atomic<JsonDocument*> pending{nullptr};
public:
  ~ConfigPatchQueue(){delete pending.load();}
  bool busy() const{return pending.load()!=nullptr;}
  ConfigQueueResult submit(JsonDocument &doc)
  {
    auto *next=new(std::nothrow) JsonDocument(std::move(doc));
    if(!next) return ConfigQueueResult::no_memory;
    JsonDocument *empty=nullptr;
    if(!pending.compare_exchange_strong(empty,next)) {delete next;return ConfigQueueResult::busy;}
    return ConfigQueueResult::queued;
  }
  JsonDocument *peek() const{return pending.load();}
  void complete(JsonDocument *used)
  {
    if(pending.compare_exchange_strong(used,nullptr)) delete used;
  }
};

// Apply only submitted settings to the state that exists NOW. A queued full AppConfig copy would
// roll back holds, detected model/corrections, telemetry IDs or any other intervening runtime update.
inline void configApplySettingsPatch(AppConfig &live,JsonVariantConst patch)
{
  char oldModel[sizeof(live.model)];memcpy(oldModel,live.model,sizeof(oldModel));
  configFromJson(live,patch);
  if(strcmp(oldModel,live.model)!=0 && !patch["hp"]["layout_fixes"].is<JsonArrayConst>()) live.fixCount=0;
}
#endif
