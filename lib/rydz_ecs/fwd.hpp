#pragma once

#include <cstdint>
#include <typeindex>
#include <typeinfo>

namespace ecs {

struct BundleType {};
struct ComponentType {};
struct ResourceType {};

struct Entity;
class EntityManager;

struct Tick;
struct ComponentTicks;


struct SystemAccess;

class IStorage;
template <typename T> class SparseSetStorage;
template <typename T> class HashMapStorage;

class Resources;

class World;

template <typename T> struct Mut;
template <typename Inner> struct Opt;
template <typename T> struct With;
template <typename T> struct Without;
template <typename T> struct Added;
template <typename T> struct Changed;
template <typename F1, typename F2> struct Or;
template <typename... Fs> struct Filters;
template <typename... Qs> class Query;

class ISystem;
template <typename T> struct Res;
template <typename T> struct ResMut;

class CommandQueue;
class Cmd;
class EntityCommands;

template <typename E> class Events;
template <typename E> class EventWriter;
template <typename E> class EventReader;

class ICondition;

enum class ScheduleLabel;
class Schedule;
class Schedules;

template <typename S> struct State;
template <typename S> struct NextState;

struct Parent;
struct Children;

class IPlugin;

class App;

struct Time;

} // namespace ecs
