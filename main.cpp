#include <iostream>
#include <vector>
#include <algorithm>

struct Pillar;

struct Cell
{
    Pillar* source = nullptr;
    Pillar* target = nullptr;
    Cell* reverse = nullptr;    // reverse cell for symmetrical one: ban reverse moves
    unsigned dist = 0;
    bool isSymmetrical = false;
    mutable unsigned nBans = 0;
};

struct Pillar
{
    const unsigned index, initial, required;
    std::vector<Pillar*> adjacent;

    unsigned current = 0;
    std::vector<Cell> cells;

    Pillar(unsigned aIndex, unsigned aInitial, unsigned aRequired)
        : index(aIndex), initial(aInitial), required(aRequired) {}
};

struct Maze
{
    std::vector<Pillar> pillars;
    unsigned nBadPillars = 0;
    unsigned wantedSolution = 0;
    unsigned nPillars() const { return pillars.size(); }

    void addPillar(unsigned initial, unsigned required);
    void addMonoLink(unsigned i1, unsigned i2);
    void addBiLink(unsigned i1, unsigned i2);

    void checkSums() const;
    void preprocess();
    std::vector<const Cell*> solve(unsigned nWantedMoves);
    unsigned recurse(
            std::vector<const Cell*>& moves,
            unsigned iMove);
    void outPreprocessed();
    /// @return [+] can move
    bool doMove(const Cell& x);
    void undoMove(const Cell& x);
};

void Maze::addPillar(unsigned initial, unsigned required)
{
    pillars.emplace_back(nPillars(), initial, required);
}

void Maze::addMonoLink(unsigned i1, unsigned i2)
{
    auto& p1 = pillars.at(i1);
    auto& p2 = pillars.at(i2);
    p1.adjacent.emplace_back(&p2);
}

void Maze::addBiLink(unsigned i1, unsigned i2)
{
    auto& p1 = pillars.at(i1);
    auto& p2 = pillars.at(i2);
    p1.adjacent.emplace_back(&p2);
    p2.adjacent.emplace_back(&p1);
}

void Maze::checkSums() const
{
    unsigned nInitial = 0;
    unsigned nRequired = 0;
    for (auto& p : pillars) {
        nInitial += p.initial;
        nRequired += p.required;
    }
    if (nInitial != nRequired)
        throw std::logic_error("Initial != required");
}

void Maze::preprocess()
{
    constexpr unsigned BIG_DIST = 10000;
    unsigned n = nPillars();
    nBadPillars = 0;
    // Initial fill
    for (auto& p : pillars) {
        p.current = p.initial;
        if (p.initial != p.required)
            ++nBadPillars;
        p.cells.resize(n);
        for (unsigned j = 0; j < n; ++j) {
            auto& cell = p.cells[j];
            auto& q = pillars[j];
            cell.source = &p;
            cell.target = &q;
            cell.dist = BIG_DIST;
        }
    }
    // Floyd-Warshall algo — init
    for (auto& u : pillars) {
        for (auto& v : u.adjacent) {
            u.cells[v->index].dist = 1;
        }
        u.cells[u.index].dist = 0;
    }
    // Floyd-Warshall algo — run
    for (auto& i : pillars) {
        for (auto& u : pillars) {
            for (auto& v : u.cells) {
                v.dist = std::min(v.dist,
                    u.cells[i.index].dist + i.cells[v.target->index].dist);
            }
        }
    }
    // Check for symmetry
    for (auto& u : pillars) {
        for (auto& v : pillars) {
            auto& cell1 = u.cells[v.index];
            auto& cell2 = v.cells[u.index];
            if (cell1.dist == cell2.dist) {
                cell1.isSymmetrical = true;
                cell2.isSymmetrical = true;
            }
        }
    }
    // Cut matrix
    for (auto& u : pillars) {
        auto end2 = std::remove_if(
                u.cells.begin(), u.cells.end(),
                [](const Cell& cell) {
                    return (cell.dist == 0 || cell.dist == BIG_DIST);
                });
        u.cells.erase(end2, u.cells.end());
    }
    // Find reverse cell
    for (auto& u : pillars) {
        for (auto& c : u.cells) {
            c.reverse = nullptr;
            if (c.isSymmetrical) {
                auto& v = *c.target;
                for (auto& c2 : v.cells) {
                    if (c2.target == &u) {
                        c.reverse = &c2;
                        goto brk;
                    }
                }
                throw std::logic_error("Cannot find reverse cell");
            brk:;
            }
        }
    }
}

void Maze::outPreprocessed()
{
    for (auto& p : pillars) {
        for (auto& c : p.cells) {
            std::cout << p.index << " -> " << c.target->index << " = " << c.dist;
            if (c.isSymmetrical)
                std::cout << " [SYM]";
            std::cout << std::endl;
        }
    }
}

constexpr unsigned CANNOT_SOLVE = 10000;

bool Maze::doMove(const Cell& x)
{
    // Moved A→B, this move is symmetrical → ban B→A
    if (x.nBans != 0)
        return false;

    auto& src = *x.source;
    if (src.current < x.dist)
        return false;

    if (src.current == src.required)
        ++nBadPillars;
    src.current -= x.dist;
    if (src.current == src.required)
        --nBadPillars;

    auto& tg = *x.target;
    if (tg.current == tg.required)
        ++nBadPillars;
    tg.current += x.dist;
    if (tg.current == tg.required)
        --nBadPillars;

    if (x.reverse)
        ++x.reverse->nBans;

    return true;
}

void Maze::undoMove(const Cell& x)
{
    auto& src = *x.source;
    if (src.current == src.required)
        ++nBadPillars;
    src.current += x.dist;
    if (src.current == src.required)
        --nBadPillars;

    auto& tg = *x.target;
    if (tg.current == tg.required)
        ++nBadPillars;
    tg.current -= x.dist;
    if (tg.current == tg.required)
        --nBadPillars;

    if (x.reverse)
        --x.reverse->nBans;
}

unsigned Maze::recurse(
        std::vector<const Cell*>& moves,
        unsigned iMove)
{
    if (nBadPillars == 0)
        return iMove;
    auto remainder = wantedSolution - iMove;
    if (remainder * 2 < nBadPillars)
        return CANNOT_SOLVE;
    auto i1 = iMove + 1;
    for (auto& u : pillars) {
        if (u.current != 0) {  // non-empty pillars only
            for (auto& v : u.cells) {
                if (doMove(v)) {
                    moves[iMove] = &v;
                    if (auto r = recurse(moves, i1);
                            r != CANNOT_SOLVE) {
                        return r;
                    }
                    undoMove(v);
                }
            }
        }
    }
    return CANNOT_SOLVE;
}

std::vector<const Cell*> Maze::solve(unsigned nWantedMoves)
{
    checkSums();
    preprocess();
    std::vector<const Cell*> r;
    r.resize(nWantedMoves);
    wantedSolution = nWantedMoves;
    //outPreprocessed();

    auto nMoves = recurse(r, 0);

    if (nMoves == CANNOT_SOLVE) {
        return {};
    } else {
        r.resize(nMoves);
        return r;
    }
}

void outSolution(const std::vector<const Cell*>& x)
{
    std::cout << "Solution size: " << x.size() << std::endl;
    for (auto v : x) {
        if (v) {
            std::cout << v->source->index << " -> " << v->target->index << " = " << v->dist << std::endl;
        } else {
            std::cout << "NULL" << std::endl;
        }
    }
}

void solve95()
{
    Maze maze;
    // Pillars, ANTICLOCKWISE from TOP
    maze.addPillar(4, 9);
    maze.addPillar(4, 3);
    maze.addPillar(1, 0);
    maze.addPillar(7, 9);
    maze.addPillar(1, 3);
    maze.addPillar(7, 0);
    // Links
    maze.addBiLink(0, 1);
    maze.addBiLink(1, 2);
    maze.addBiLink(2, 3);
    maze.addBiLink(3, 4);
    maze.addBiLink(4, 5);
    maze.addBiLink(5, 0);
    // Go!
    auto r = maze.solve(7);
    outSolution(r);
}

void solve96()
{
    Maze maze;
    // Pillars, TOP row, then BOTTOM
    maze.addPillar(0, 0);
    maze.addPillar(0, 0);
    maze.addPillar(0, 0);
    maze.addPillar(2, 0);
    maze.addPillar(0, 6);
    maze.addPillar(0, 0);
    maze.addPillar(1, 0);
    maze.addPillar(3, 0);
    // Links
    maze.addBiLink(4, 0);
    maze.addBiLink(0, 1);
    maze.addBiLink(1, 5);
    maze.addBiLink(5, 6);
    maze.addBiLink(6, 2);
    maze.addBiLink(6, 3);
    maze.addBiLink(3, 7);
    // Go!
    auto r = maze.solve(4);
    outSolution(r);
}

void solve97()
{
    Maze maze;
    // Pillars, TOP row
    maze.addPillar(2, 0);
    // MID row
    maze.addPillar(3, 0);
    maze.addPillar(0, 4);
    maze.addPillar(2, 6);
    // BOTTOM row
    maze.addPillar(0, 1);
    maze.addPillar(5, 0);
    maze.addPillar(0, 1);
    // Links
    maze.addBiLink(0, 2);
    maze.addBiLink(1, 2);
    maze.addMonoLink(3, 2);
    maze.addBiLink(1, 4);
    maze.addBiLink(4, 5);
    maze.addBiLink(5, 6);
    maze.addBiLink(3, 6);
    // Go!
    auto r = maze.solve(6);
    outSolution(r);
}

void solve100()
{
    Maze maze;
    // Pillars, TOP
    maze.addPillar(0, 4);   // 0
    // 2nd row
    maze.addPillar(0, 4);   // 1
    maze.addPillar(0, 4);   // 2
    // MAIN row
    maze.addPillar(0, 0);   // 3
    maze.addPillar(0, 0);   // 4
    maze.addPillar(0, 0);   // 5
    maze.addPillar(0, 0);   // 6
    // BOTTOM row
    maze.addPillar(6, 0);   // 7
    maze.addPillar(6, 0);   // 8
    // Links
    maze.addMonoLink(1, 0);
    maze.addMonoLink(2, 0);
    maze.addMonoLink(3, 1);
    maze.addMonoLink(6, 2);
    maze.addBiLink(3, 4);
    maze.addBiLink(4, 5);
    maze.addBiLink(5, 6);
    maze.addMonoLink(7, 3);
    maze.addMonoLink(8, 6);
    // Go!
    auto r = maze.solve(8);
    outSolution(r);
}

int main()
{
    solve100();
}
