// Jolt.h must precede every other Jolt header.
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace JPH;
namespace {
struct Touch {
  std::string Other, Tag, Type;
  bool Sensor = false;
  float X = 0, Y = 0, Z = 0, Speed = 0;
};
struct State {
  std::string Name, Tag;
  BodyID Id;
  uint64_t Category = 1, Mask = UINT64_MAX;
  std::vector<Touch> Contacts, Events, Pending;
};
std::map<std::string, State> Bodies;
float Units = 1;
class Broad final : public BroadPhaseLayerInterface {
public:
  uint GetNumBroadPhaseLayers() const override { return 2; }
  BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override {
    return BroadPhaseLayer(l);
  }
} BroadLayers;
class ObjectBroad final : public ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
    return a != 0 || b.GetValue() != 0;
  }
} ObjectBroadFilter;
class Layers final : public ObjectLayerPairFilter {
public:
  bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
    return a != 0 || b != 0;
  }
} LayerFilter;
std::unique_ptr<PhysicsSystem> World;
std::unique_ptr<TempAllocatorImpl> Allocator;
std::unique_ptr<JobSystemSingleThreaded> Jobs;
struct Cached {
  std::string A, B;
  Touch ToA, ToB;
};
std::map<std::pair<uint32, uint32>, Cached> Contacts;
State &Find(const std::string &n) {
  auto i = Bodies.find(n);
  if (i == Bodies.end())
    throw std::runtime_error("Physics body not attached: " + n);
  return i->second;
}
State &Owner(const Body &b) {
  return *reinterpret_cast<State *>(b.GetUserData());
}
void Emit(const Cached &c, const std::string &type) {
  auto a = Bodies.find(c.A), b = Bodies.find(c.B);
  if (a != Bodies.end()) {
    auto t = c.ToA;
    t.Type = type;
    a->second.Events.push_back(t);
  }
  if (b != Bodies.end()) {
    auto t = c.ToB;
    t.Type = type;
    b->second.Events.push_back(t);
  }
}
class Listener final : public ContactListener {
  ValidateResult OnContactValidate(const Body &a, const Body &b, RVec3Arg,
                                   const CollideShapeResult &) override {
    auto &x = Owner(a);
    auto &y = Owner(b);
    return (x.Category & y.Mask) && (y.Category & x.Mask)
               ? ValidateResult::AcceptAllContactsForThisBodyPair
               : ValidateResult::RejectAllContactsForThisBodyPair;
  }
  void Save(const Body &a, const Body &b, const ContactManifold &m,
            bool begin) {
    auto &x = Owner(a);
    auto &y = Owner(b);
    auto n = m.mWorldSpaceNormal;
    bool sensor = a.IsSensor() || b.IsSensor();
    float speed =
        std::max(0.f, (a.GetLinearVelocity() - b.GetLinearVelocity()).Dot(n)) *
        Units;
    Cached c{
        x.Name,
        y.Name,
        {y.Name, y.Tag, "", sensor, -n.GetX(), -n.GetY(), -n.GetZ(), speed},
        {x.Name, x.Tag, "", sensor, n.GetX(), n.GetY(), n.GetZ(), speed}};
    Contacts[{a.GetID().GetIndexAndSequenceNumber(),
              b.GetID().GetIndexAndSequenceNumber()}] = c;
    if (begin) {
      Emit(c, "Begin");
      if (!sensor && speed > Units)
        Emit(c, "Hit");
    }
  }
  void OnContactAdded(const Body &a, const Body &b, const ContactManifold &m,
                      ContactSettings &) override {
    Save(a, b, m, true);
  }
  void OnContactPersisted(const Body &a, const Body &b,
                          const ContactManifold &m,
                          ContactSettings &) override {
    Save(a, b, m, false);
  }
  void OnContactRemoved(const SubShapeIDPair &p) override {
    auto i = Contacts.find({p.GetBody1ID().GetIndexAndSequenceNumber(),
                            p.GetBody2ID().GetIndexAndSequenceNumber()});
    if (i != Contacts.end()) {
      Emit(i->second, "End");
      Contacts.erase(i);
    }
  }
} ContactListenerInstance;
void Ensure() {
  if (World)
    return;
  RegisterDefaultAllocator();
  Factory::sInstance = new Factory;
  RegisterTypes();
  Allocator = std::make_unique<TempAllocatorImpl>(32 * 1024 * 1024);
  Jobs = std::make_unique<JobSystemSingleThreaded>(2048);
  World = std::make_unique<PhysicsSystem>();
  World->Init(16384, 0, 65536, 16384, BroadLayers, ObjectBroadFilter,
              LayerFilter);
  World->SetGravity(Vec3(0, -9.8f, 0));
  World->SetContactListener(&ContactListenerInstance);
}
BodyInterface &Interface() {
  Ensure();
  return World->GetBodyInterface();
}
void Finite(float x, float y, float z = 0) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    throw std::runtime_error("Non-finite physics value");
}
float Number(const std::string &s) {
  size_t used;
  float n = std::stof(s, &used);
  Finite(n, 0);
  if (used != s.size())
    throw std::runtime_error("Invalid physics number");
  return n;
}
void ShapeFor(std::string n, const Shape *shape) {
  Interface().SetShape(Find(n).Id, shape, true, EActivation::Activate);
}
} // namespace
extern "C" void Constructor() { Ensure(); }
extern "C" void Destructor() {
  Contacts.clear();
  World.reset();
  Bodies.clear();
  Jobs.reset();
  Allocator.reset();
  if (Factory::sInstance) {
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
  }
  Units = 1;
}
extern "C" void Debug(bool) {}
extern "C" void Set_Paths(std::string) {}
extern "C" void Attach(std::string n) {
  Ensure();
  if (Bodies.count(n))
    return;
  auto &b = Bodies[n];
  b.Name = n;
  BodyCreationSettings d(new EmptyShape, RVec3::sZero(), Quat::sIdentity(),
                         EMotionType::Dynamic, 1);
  d.mUserData = reinterpret_cast<uint64>(&b);
  d.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY |
                   EAllowedDOFs::TranslationZ;
  d.mAllowSleeping = false;
  d.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
  d.mMassPropertiesOverride.mMass = 1;
  b.Id = Interface().CreateAndAddBody(d, EActivation::Activate);
  if (b.Id.IsInvalid()) {
    Bodies.erase(n);
    throw std::runtime_error("Jolt body capacity exceeded");
  }
}
extern "C" void Detach(std::string n) {
  auto i = Bodies.find(n);
  if (i == Bodies.end())
    return;
  auto id = i->second.Id;
  Interface().RemoveBody(id);
  Interface().DestroyBody(id);
  Bodies.erase(i);
  for (auto j = Contacts.begin(); j != Contacts.end();)
    if (j->second.A == n || j->second.B == n) {
      auto c = j->second;
      auto a = Bodies.find(c.A), b = Bodies.find(c.B);
      if (a != Bodies.end()) {
        auto t = c.ToA;
        t.Type = "End";
        a->second.Pending.push_back(t);
      }
      if (b != Bodies.end()) {
        auto t = c.ToB;
        t.Type = "End";
        b->second.Pending.push_back(t);
      }
      j = Contacts.erase(j);
    } else
      ++j;
  for (auto &p : Bodies) {
    auto &c = p.second.Contacts;
    c.erase(std::remove_if(c.begin(), c.end(),
                           [&](const Touch &t) { return t.Other == n; }),
            c.end());
  }
}
extern "C" void Set_Units_Per_Meter(float n) {
  if (!std::isfinite(n) || n <= 0 || !Bodies.empty())
    throw std::runtime_error("Set units before attaching physics bodies");
  Units = n;
}
extern "C" void Set_Box_3D(std::string n, float x, float y, float z) {
  Finite(x, y, z);
  if (x <= 0 || y <= 0 || z <= 0)
    throw std::runtime_error("Box dimensions must be positive");
  ShapeFor(n, new BoxShape(Vec3(x, y, z) / (2 * Units)));
}
extern "C" void Set_Sphere(std::string n, float r) {
  Finite(r, 0);
  if (r <= 0)
    throw std::runtime_error("Sphere radius must be positive");
  ShapeFor(n, new SphereShape(r / Units));
}
extern "C" void Set_Capsule(std::string n, float r, float h) {
  Finite(r, h);
  if (r <= 0 || h < 2 * r)
    throw std::runtime_error("Capsule height must be at least its diameter");
  if (h == 2 * r) {
    Set_Sphere(n, r);
    return;
  }
  ShapeFor(n, new CapsuleShape((h / 2 - r) / Units, r / Units));
}
extern "C" void Set_Body_Type(std::string n, std::string type) {
  EMotionType t;
  if (type == "Static")
    t = EMotionType::Static;
  else if (type == "Dynamic")
    t = EMotionType::Dynamic;
  else if (type == "Kinematic")
    t = EMotionType::Kinematic;
  else
    throw std::runtime_error("Unknown body type: " + type);
  auto id = Find(n).Id;
  Interface().SetMotionType(id, t, EActivation::Activate);
  Interface().SetObjectLayer(id, t == EMotionType::Static ? 0 : 1);
}
extern "C" void Set_Position(std::string n, float x, float y, float z) {
  Finite(x, y, z);
  Interface().SetPosition(Find(n).Id, RVec3(x / Units, y / Units, z / Units),
                          EActivation::Activate);
}
extern "C" void Set_Velocity(std::string n, float x, float y, float z) {
  Finite(x, y, z);
  Interface().SetLinearVelocity(Find(n).Id, Vec3(x, y, z) / Units);
}
extern "C" void Apply_Force(std::string n, float x, float y, float z) {
  Finite(x, y, z);
  Interface().AddForce(Find(n).Id, Vec3(x, y, z) / Units);
}
extern "C" void Apply_Impulse(std::string n, float x, float y, float z) {
  Finite(x, y, z);
  Interface().AddImpulse(Find(n).Id, Vec3(x, y, z) / Units);
}
extern "C" void Set_Gravity(float x, float y, float z) {
  Finite(x, y, z);
  Ensure();
  World->SetGravity(Vec3(x, y, z) / Units);
}
extern "C" void Set(std::string n, std::string key, std::string value) {
  auto &b = Find(n);
  if (key == "Tag") {
    b.Tag = value;
    return;
  }
  if (key == "Category" || key == "Mask") {
    size_t used;
    auto v = std::stoull(value, &used, 0);
    if (used != value.size() || value[0] == '-')
      throw std::runtime_error("Invalid collision filter");
    if (key == "Category")
      b.Category = v;
    else
      b.Mask = v;
    Interface().InvalidateContactCache(b.Id);
    Interface().ActivateBody(b.Id);
    return;
  }
  float v = Number(value);
  auto &api = Interface();
  if (key == "Sensor")
    api.SetIsSensor(b.Id, v != 0);
  else if (key == "Friction" && v >= 0)
    api.SetFriction(b.Id, v);
  else if (key == "Restitution" && v >= 0 && v <= 1)
    api.SetRestitution(b.Id, v);
  else if (key == "Gravity_Scale")
    api.SetGravityFactor(b.Id, v);
  else if (key == "Bullet")
    api.SetMotionQuality(b.Id, v != 0 ? EMotionQuality::LinearCast
                                      : EMotionQuality::Discrete);
  else if (key == "Mass" && v > 0) {
    BodyLockWrite lock(World->GetBodyLockInterface(), b.Id);
    if (lock.Succeeded()) {
      auto *motion = lock.GetBody().GetMotionProperties();
      if (!motion)
        throw std::runtime_error("Mass requires moving body");
      auto props = lock.GetBody().GetShape()->GetMassProperties();
      props.ScaleToMass(v);
      motion->SetMassProperties(
          lock.GetBody().GetMotionProperties()->GetAllowedDOFs(), props);
    }
  } else
    throw std::runtime_error("Unsupported physics setting or value: " + key);
}
extern "C" void Step(float seconds) {
  if (!std::isfinite(seconds) || seconds <= 0 || seconds > .1f)
    throw std::runtime_error("Physics Step must be in (0, 0.1] seconds");
  Ensure();
  for (auto &p : Bodies) {
    p.second.Events = std::move(p.second.Pending);
    p.second.Pending.clear();
    p.second.Contacts.clear();
  }
  auto error = World->Update(seconds, 4, Allocator.get(), Jobs.get());
  if (error != EPhysicsUpdateError::None)
    throw std::runtime_error("Jolt simulation capacity exceeded");
  for (auto &p : Contacts) {
    auto &c = p.second;
    auto a = Bodies.find(c.A), b = Bodies.find(c.B);
    if (a != Bodies.end() && b != Bodies.end()) {
      a->second.Contacts.push_back(c.ToA);
      b->second.Contacts.push_back(c.ToB);
    }
  }
}
extern "C" void Update(std::string) { Step(1.f / 60); }
extern "C" std::string Get(std::string n, std::string key) {
  auto &b = Find(n);
  auto p = Interface().GetPosition(b.Id);
  auto v = Interface().GetLinearVelocity(b.Id);
  if (key == "Position_X")
    return std::to_string(p.GetX() * Units);
  if (key == "Position_Y")
    return std::to_string(p.GetY() * Units);
  if (key == "Position_Z")
    return std::to_string(p.GetZ() * Units);
  if (key == "Velocity_X")
    return std::to_string(v.GetX() * Units);
  if (key == "Velocity_Y")
    return std::to_string(v.GetY() * Units);
  if (key == "Velocity_Z")
    return std::to_string(v.GetZ() * Units);
  if (key == "Tag")
    return b.Tag;
  if (key == "Contact_Count")
    return std::to_string(b.Contacts.size());
  if (key == "Event_Count")
    return std::to_string(b.Events.size());
  if (key == "Mass") {
    BodyLockRead lock(World->GetBodyLockInterface(), b.Id);
    float inverse = lock.Succeeded() && !lock.GetBody().IsStatic() &&
                            lock.GetBody().GetMotionProperties()
                        ? lock.GetBody().GetMotionProperties()->GetInverseMass()
                        : 0;
    return std::to_string(inverse > 0 ? 1 / inverse : 0);
  }
  if (key == "Grounded") {
    auto g = World->GetGravity();
    float len = g.Length();
    for (auto &c : b.Contacts)
      if (!c.Sensor && len > 0 && Vec3(c.X, c.Y, c.Z).Dot(g) / len < -.5f)
        return "1";
    return "0";
  }
  throw std::runtime_error("Unknown physics field: " + key);
}
namespace {
std::string Field(const Touch &t, const std::string &f) {
  if (f == "Other")
    return t.Other;
  if (f == "Tag")
    return t.Tag;
  if (f == "Type")
    return t.Type;
  if (f == "Sensor")
    return t.Sensor ? "1" : "0";
  if (f == "Normal_X")
    return std::to_string(t.X);
  if (f == "Normal_Y")
    return std::to_string(t.Y);
  if (f == "Normal_Z")
    return std::to_string(t.Z);
  if (f == "Speed")
    return std::to_string(t.Speed);
  throw std::runtime_error("Unknown contact/event field: " + f);
}
} // namespace
extern "C" std::string Contact(std::string n, int i, std::string f) {
  auto &v = Find(n).Contacts;
  return i < 0 || size_t(i) >= v.size() ? "" : Field(v[i], f);
}
extern "C" std::string Event(std::string n, int i, std::string f) {
  auto &v = Find(n).Events;
  return i < 0 || size_t(i) >= v.size() ? "" : Field(v[i], f);
}
