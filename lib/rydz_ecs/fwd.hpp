#pragma once

#include <cstdint>
#include <typeindex>
#include <typeinfo>

namespace ecs {

struct Bundle {};
struct Component {};
struct Event {};
struct Resource {};
struct Set {};

struct Entity;
struct EntityManager;

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
struct NonSendMarker;
template <typename T> struct Res;
template <typename T> struct ResMut;
template <typename P> struct SystemParamTraits;

class CommandQueue;
class CommandQueues;
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
