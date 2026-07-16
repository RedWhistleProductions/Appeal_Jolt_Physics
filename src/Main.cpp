#include <iostream>
#include <map>
#include <string>

namespace
{
    struct Vec3 { float X = 0.0f; float Y = 0.0f; float Z = 0.0f; };
    struct Body
    {
        std::string Type = "Dynamic";
        float Mass = 1.0f;
        Vec3 Position;
        Vec3 Velocity;
        Vec3 Force;
    };

    bool Debug_Output = false;
    std::map<std::string, Body> Bodies;
    Vec3 Gravity = {0.0f, -9.8f, 0.0f};

    void Print(const std::string &Message)
    {
        if(Debug_Output) std::cout << Message << std::endl;
    }

    std::string Vec_To_String(const Vec3 &Value)
    {
        return std::to_string(Value.X) + ", " + std::to_string(Value.Y) + ", " + std::to_string(Value.Z);
    }

    Body &Get_Body(const std::string &Entity)
    {
        return Bodies[Entity];
    }
}

extern "C" void Constructor() { Print("Appeal_Jolt_Physics Constructor()"); }
extern "C" void Destructor() { Print("Appeal_Jolt_Physics Destructor()"); }
extern "C" void Debug(bool Enable) { Debug_Output = Enable; }
extern "C" void Set_Paths(std::string Resources) { Print("Jolt physics resources: " + Resources); }
extern "C" void Attach(std::string Entity) { Get_Body(Entity); Print("Jolt attach: " + Entity); }
extern "C" void Detach(std::string Entity) { Bodies.erase(Entity); Print("Jolt detach: " + Entity); }

extern "C" void Update(std::string Entity)
{
    Body &Body_Data = Get_Body(Entity);
    const float Dt = 1.0f / 60.0f;
    Body_Data.Velocity.X += (Gravity.X + Body_Data.Force.X / Body_Data.Mass) * Dt;
    Body_Data.Velocity.Y += (Gravity.Y + Body_Data.Force.Y / Body_Data.Mass) * Dt;
    Body_Data.Velocity.Z += (Gravity.Z + Body_Data.Force.Z / Body_Data.Mass) * Dt;
    Body_Data.Position.X += Body_Data.Velocity.X * Dt;
    Body_Data.Position.Y += Body_Data.Velocity.Y * Dt;
    Body_Data.Position.Z += Body_Data.Velocity.Z * Dt;
    Body_Data.Force = {};
    Print("Jolt update: " + Entity + " position=" + Vec_To_String(Body_Data.Position));
}

extern "C" void Set(std::string Entity, std::string Key, std::string Value)
{
    Body &Body_Data = Get_Body(Entity);
    if(Key == "Mass") Body_Data.Mass = std::stof(Value);
    Print("Jolt set: " + Entity + "." + Key + " = " + Value);
}

extern "C" void Set_Body_Type(std::string Entity, std::string Type) { Get_Body(Entity).Type = Type; Print("Jolt body type: " + Entity + " = " + Type); }
extern "C" void Set_Position(std::string Entity, float X, float Y, float Z) { Get_Body(Entity).Position = {X, Y, Z}; Print("Jolt position: " + Entity + " = " + Vec_To_String(Get_Body(Entity).Position)); }
extern "C" void Set_Velocity(std::string Entity, float X, float Y, float Z) { Get_Body(Entity).Velocity = {X, Y, Z}; Print("Jolt velocity: " + Entity + " = " + Vec_To_String(Get_Body(Entity).Velocity)); }
extern "C" void Apply_Force(std::string Entity, float X, float Y, float Z) { Body &Body_Data = Get_Body(Entity); Body_Data.Force.X += X; Body_Data.Force.Y += Y; Body_Data.Force.Z += Z; Print("Jolt force: " + Entity + " = " + Vec_To_String({X, Y, Z})); }
extern "C" void Apply_Impulse(std::string Entity, float X, float Y, float Z) { Body &Body_Data = Get_Body(Entity); Body_Data.Velocity.X += X / Body_Data.Mass; Body_Data.Velocity.Y += Y / Body_Data.Mass; Body_Data.Velocity.Z += Z / Body_Data.Mass; Print("Jolt impulse: " + Entity + " = " + Vec_To_String({X, Y, Z})); }
extern "C" void Set_Gravity(float X, float Y, float Z) { Gravity = {X, Y, Z}; Print("Jolt gravity: " + Vec_To_String(Gravity)); }
