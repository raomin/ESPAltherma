#pragma once
#include <string>
#include <vector>

// Home Assistant keeps a component that a new device discovery merely leaves out: a deselected value stayed as
// an entity with its last reading, and deleting that entity by hand cleared the whole device. A component is
// removed when it is sent with nothing but its platform ({"p":"sensor"}), then left out of the next update
// (https://www.home-assistant.io/integrations/mqtt/#device-discovery-payload).
//
// No Arduino dependency, so a unit test drives it.

struct HaComponent
{
  std::string key;      // the component key, which the unique id is built from
  std::string platform; // "sensor", "binary_sensor"
};

class HaAnnounced
{
public:
  // The components of the last discovery sent that `now` no longer has
  std::vector<HaComponent> removed(const std::vector<HaComponent> &now) const
  {
    std::vector<HaComponent> out;
    for (const HaComponent &before : last)
    {
      bool kept = false;
      for (const HaComponent &c : now)
        kept = kept || c.key == before.key;
      if (!kept)
        out.push_back(before);
    }
    return out;
  }

  // Records the components of a discovery once it is sent: the next one removes what this one had and it lacks.
  // Lost at a restart, so a value deselected while the board is off still stays in Home Assistant.
  void sent(const std::vector<HaComponent> &now) { last = now; }

private:
  std::vector<HaComponent> last;
};

// The "key":{"p":"platform"} element that removes a component, with its leading comma
inline std::string haRemovalJson(const HaComponent &c)
{
  return ",\"" + c.key + "\":{\"p\":\"" + c.platform + "\"}";
}
