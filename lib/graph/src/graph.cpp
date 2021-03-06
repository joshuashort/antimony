#include <algorithm>

#include "graph/graph.h"
#include "graph/node.h"
#include "graph/proxy.h"
#include "graph/hooks/hooks.h"
#include "graph/watchers.h"

Graph::Graph(std::string n, Graph* parent)
    : name(n), uid(0), parent(parent), processing_queue(false)
{
    // Nothing to do here
}

PyObject* Graph::proxyDict(Datum* caller)
{
    return Proxy::makeProxyFor(this, caller);
}

void Graph::triggerWatchers() const
{
    if (!watchers.empty())
    {
        auto state = getState();
        for (auto w : watchers)
            w->trigger(state);
    }
}

uint32_t Graph::install(Node* n)
{
    return Root::install(n, &nodes);
}

GraphState Graph::getState() const
{
    std::unordered_set<Node*> out;
    for (const auto& ptr : nodes)
        out.insert(ptr.get());
    return (GraphState){ out };
}

bool Graph::isNameUnique(std::string name, const Node* n) const
{
    for (const auto& a : nodes)
        if (a->name == name)
            return (n == a.get());
    return true;
}

std::string Graph::nextName(std::string prefix) const
{
    unsigned i=0;
    while(1)
    {
        auto n = prefix + std::to_string(i++);
        if (isNameUnique(n))
            return n;
    }
}

std::list<uint64_t> Graph::getUIDs(unsigned n) const
{
    std::list<uint64_t> out;
    std::unordered_set<uint64_t> existing;

    // Load all of the existing node's UIDs into a set
    for (const auto& node : nodes)
        existing.insert(node->getUID());

    // Count up, filling up the output list with unused UIDs
    uint64_t u = 0;
    while (out.size() < n)
    {
        if (existing.count(u) == 0)
            out.push_back(u);
        u++;
    }

    return out;
}

void Graph::uninstall(Node* n)
{
    Root::uninstall(n, &nodes);
    triggerWatchers();
}

void Graph::clear()
{
    nodes.clear();
    triggerWatchers();
}

void Graph::loadScriptHooks(PyObject* g, Node* n)
{
    if (external)
        external->loadScriptHooks(g, n);
}

std::list<Node*> Graph::childNodes() const
{
    std::list<Node*> out;
    for (const auto& ptr : nodes)
        out.push_back(ptr.get());
    return out;
}

void Graph::loadDatumHooks(PyObject* g)
{
    if (external)
        external->loadDatumHooks(g);
}

PyObject* Graph::pyGetAttr(std::string name, Downstream* caller) const
{
    auto m = get(name, nodes);
    return m ? Proxy::makeProxyFor(m, caller) : NULL;
}

void Graph::queue(Downstream* d)
{
    downstream_queue.insert(d);
}

void Graph::flushQueue()
{
    if (!processing_queue)
    {
        processing_queue = true;
        while (downstream_queue.size())
        {
            Downstream* next = NULL;
            for (auto d : downstream_queue)
            {
                auto sources = d->sources;
                sources.erase(d);
                if (!std::any_of(sources.begin(), sources.end(),
                        [&](const Downstream* s){
                            return downstream_queue.count(
                                const_cast<Downstream*>(s));
                        }))
                {
                    next = d;
                    break;
                }
            }

            assert(next);
            downstream_queue.erase(next);
            next->trigger();
        }

        processing_queue = false;
    }
}

void Graph::preInit()
{
    Proxy::preInit();
    Hooks::preInit();
}
