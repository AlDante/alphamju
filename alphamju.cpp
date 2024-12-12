/*
   Alphamju - on the way to a single Dummy Solver
   Copyright © 2024 by Kalle Prorok. All rights reserved.

   NOT READY YET - WORK IN PROGRESS

   Based on the paper
   The alpha mju Search Algorithm for the Game of Bridge (greek letters replaced)
   Tristan Cazenave and Veronique Ventos 2019

   Uses DDS, a bridge double dummy solver 
   by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dll.h"
#include "hands.h"
#include <set>
#include <list>
#include <chrono>
#include <map>
#include <boost/predef.h> // For identifying which OS we're compiling for
#if !defined(BOOST_OS_WINDOWS) && !defined(BOOST_OS_MACOS)
#include "strlcpy.h"
#endif

//#include <algorithm>
//#include <execution>

//#include <ppl.h>
//using namespace concurrency;
#include <omp.h>
#include "hands.cpp" // To get definitions and useful functions
#include <cstring>

using namespace std;
using namespace std::chrono;

#define WORLDSIZE 4

char Worlds[WORLDSIZE][80] = {
"W:JT98.Q54.T9.2 765.AJT9.765. 432.32.8432.3 AKQ.K876.AKQ."      ,
"W:JT984.54.T9.2 765.AJT9.765. 32.Q32.8432.3 AKQ.K876.AKQ."      ,
"W:JT98.Q53.T9.2 765.AJT9.765. 432.42.8432.3 AKQ.K876.AKQ."      ,
"W:JT984.53.T9.2 765.AJT9.765. 32.Q42.8432.3 AKQ.K876.AKQ."      ,
//"W:JT98.Q54.JT9.JT9 765.AJT9.765.765 432.32.8432.8432 AKQ.K876.AKQ.AKQ"      ,
//"W:JT984.54.JT9.JT9 765.AJT9.765.765 32.Q32.8432.8432 AKQ.K876.AKQ.AKQ"      ,
//"W:.Q76.K.9 .AJT.A.A .543.J.8 .K98.5.5"      ,
//"W:.543.K.9 .AJT.A.A .Q76.J.8 .K98.5.5"      ,
//  "W:.Q76.. .AJT.. .543.. .K98.."      ,
//  "W:.543.. .AJT.. .Q76.. .K98.."      ,
};

unsigned int holdingsc[WORLDSIZE][DDS_HANDS][DDS_SUITS];

int cards = 10; // No of cards in each Deal
int contract = 10; // 12;
int trumpc = HEARTS; //  NOTRUMP;
int firstc = SOUTH;
int dealerc = EAST;
int vulc = VUL_NONE;

int M0;


struct State
{
    bool Min_node;
    short declarerTricks;
    short defenseTricks;
    short handtoplay;
    short handsplayed;
    short firstc;
    short playno;
    short playSuit[3];
    short playRank[3];
    short playedby[3];
    unsigned int mask[4][4]; // All played cards up to now
};

struct Card
{
    short suit;
    short rank;
    short player;
};

struct Result
{
    char r[WORLDSIZE+1]; // Result, add one extra for \0
//    char m[WORLDSIZE+1]; // Mask for possible worlds - removed and use 1x0 as r instead
    struct Card card; // Store card resulting in this
};


inline bool operator<(const Result& L, const Result& R)
{
    return L.r[0] < R.r[0];
}

struct Playable
{
    short suit;
    short rank;
    short player;
    short world;
};


inline bool operator<(const Card& L, const Card& R)
{
    if (L.suit < R.suit)
        return true;
    else if (L.suit > R.suit)
        return false;
    else if (L.rank < R.rank)
        return true;
    else return false;
}

void clearresult(Result& r)
{
    for (int i = 0; i < WORLDSIZE; i++)
        r.r[i] = 'x'; // Assume everything is unknown yet
    r.r[WORLDSIZE] = '\0'; // End of string for printing
}

inline bool operator==(const Result& L, const Result& R)
{
    for (int i = 0; i < WORLDSIZE; i++)
        if (L.r[i] != R.r[i])
            return false;
    return true;
}

set<Playable> legalMoves(short int world, State& state)
{ // Find a list of possible moves
    set<Playable> moves;
    Playable p;
    p.world = world;
    if (state.playno>0 && holdingsc[world][state.handtoplay][state.playSuit[0]] > 0) 
    { // There has been card played; if possible: Follow suit
        short int s = state.playSuit[0];
        p.suit = s;
        p.player = state.handtoplay;
        unsigned int m = RA;
        bool adjacent = false;
        for (int r = 14; r > 1; r--) // Probably can be done quicker than try&error..
        {
            if ((holdingsc[world][state.handtoplay][s] & ~state.mask[state.handtoplay][s] ) & m)
            {
                if (!adjacent)
                {
                    p.rank = r;
                    p.world = world;
                    moves.insert(p);
                    adjacent = true;
                }
            }
            else adjacent = false;
            m >>= 1;
        }
    }
    else // Leading any of avalailable cards
    {
        for (short int s = 0; s < DDS_SUITS; s++)
            {
                p.suit = s;
                p.player = state.handtoplay;
                unsigned int m = RA;
                bool adjacent = false;
                for (short int r = 14; r >1; r--)
                {
                    if ((holdingsc[world][state.handtoplay][s] & ~state.mask[state.handtoplay][s]) & m)
                    {
                        if (!adjacent)
                        {
                            p.rank = r; 
                            p.world = world;
                            moves.insert(p);
                            adjacent = true;
                        }
                    }
                    else adjacent = false;
                    m >>= 1;
                }
            }
    }
    return moves;
}

inline bool operator<(const Playable& L,const Playable& R)
{
    if (L.suit < R.suit)
        return true;
    else if (L.suit > R.suit)
        return false;
    else if (L.rank < R.rank)
        return true;
    else if (L.rank > R.rank)
        return false;
    else if (L.world < R.world)
        return true;
    else return false;
}


int init()
{
    for (int i =0; i< WORLDSIZE; i++)
        ConvertPBN(Worlds[i], holdingsc[i]); // Translate PBN to BIN representation
    return 0;
}

void binprint(int v)
{
    unsigned int mask = 1 << ((sizeof(int) << 3) - 1);
    while (mask) {
        printf("%d", (v & mask ? 1 : 0));
        mask >>= 1;
    }
}

void printworlds(set<short int> worlds)
{
    for (short int w : worlds)
        printf("(W%d) ", w);
}
void printresult(Result& elem)
{
    printf("[%s]", elem.r);
}

void printresults(list<Result>& r, const char msg[])
{
    printf("%s:<", msg);
    for (Result elem : r) printresult(elem);
    printf(">");
}

void showcard(short int suit, short int rank)
{
  printf("Play %c%c ", "SHDC"[suit], dcardRank[rank]);
}

void showmove(Playable move)
{
    showcard(move.suit, move.rank);
}

int doubleDummy(short int world, const State& state)
{
    deal dl;
    futureTricks fut; 
    int target;
    int solutions;
    int mode;
    int threadIndex = 0;
    int res;
    char line[80];// , lin[80];

    #if BOOST_OS_WINDOWS
        strcpy_s(line, sizeof(line)), "DD:"); //Double Dummy result
    # else
        strlcpy(line, "DD:", sizeof(line));
    #endif
    dl.trump = trumpc;// [handno] ;
    dl.first = state.firstc; //  handtoplay; // [handno] ; // TODO Correct player!

    dl.currentTrickSuit[0] = state.playSuit[0]; 
    dl.currentTrickSuit[1] = state.playSuit[1];  
    dl.currentTrickSuit[2] = state.playSuit[2];

    dl.currentTrickRank[0] = state.playRank[0]; 
    dl.currentTrickRank[1] = state.playRank[1]; 
    dl.currentTrickRank[2] = state.playRank[2];
    //showcard(state.playSuit[0], state.playRank[0]);
        for (int h = 0; h < DDS_HANDS; h++)
            for (int s = 0; s < DDS_SUITS; s++)
                dl.remainCards[h][s] = holdingsc[world][h][s] & ~state.mask[h][s]; // Remove played cards

    target = -1;    solutions = 1;    mode = 1;
    res = SolveBoard(dl, target, solutions, mode, &fut, threadIndex);

    if (res != RETURN_NO_FAULT)
    {
        ErrorMessage(res, line);
        printf("DDS error: %s\n", line);
        printf(" first:%c Wrong hand:", "NESW"[dl.first]);
        PrintHand(line, dl.remainCards);
    }
//    printf("DD in World %d: \n", world);
//    PrintFut(line, &fut);
//    PrintHand(line, dl.remainCards);
    return fut.score[0];
}

int cardsleft(State& state)
{
    return cards - state.declarerTricks - state.defenseTricks;
}

bool stopc(State& state, int M, set<short int>& worlds, Result& result)
{
    if (state.declarerTricks >= contract)
    {
        for (short int w : worlds)
        {
            result.r[w] = '1';
        }
        return true;
    }
    else if (state.defenseTricks > cards - contract)
    {
        for (short int w : worlds)
        {
            result.r[w] = '0'; // result.m[w] = '1';
        }
        return true;
    }
    else if (M == 0)
    {
        int ns_tricks,ew_tricks;
        for (short int w : worlds)
        {
            int tricks = doubleDummy(w, state);
            if (state.Min_node)
            {
                ns_tricks = cardsleft(state) - tricks;
                ew_tricks = tricks;
            }
            else
            {
                ns_tricks = tricks;
                ew_tricks = cardsleft(state) - tricks;
            }
           // result.m[w] = '1';
            if (state.declarerTricks + ns_tricks >= contract)
            {
                result.r[w] = '1';
            }
            else
            {
                result.r[w] = '0';
            }
//            printf("DD:%d; NS took %d tricks and EW %d in W%d.\n", tricks,state.declarerTricks + ns_tricks,
//                state.defenseTricks+ew_tricks,w);
        }
//        printworlds(worlds);
//        printresult(result);
        return true;
    }
    else return false;
}

bool dominate(const Result& L, const Result& R) // Return true if L dominate (is always >=) R
{
    bool none_yet = true; // Is no one item strictly larger yet?
    for (int i = 0; i < WORLDSIZE; i++)
    {
        if (L.r[i] == '0' && R.r[i] == '1')
        {
            return false;
        }
        else if (L.r[i] == '0' && R.r[i] == 'x')
        {
            return false;
        }
        else if (L.r[i] == 'x' && R.r[i] == '1') // If any world is better it doesn't dominate
            return false;
        else if ( none_yet &&  // 1 > x > 0
            (
            (L.r[i] == '1' && (R.r[i] == '0' || R.r[i] == 'x')) || 
            (L.r[i] == 'x' && R.r[i] == '0'))
                )
            none_yet = false; // Found a strictly larger element
    }
    if (none_yet)
        return false;
    else return true;
}

bool lte(const list<Result>& front, const list<Result> f)
{
    //TODO
    return true;
}
list<Result> remove_vectors_lte(list<Result>& result, Result r)
{
    list<Result>outresult;
    for (Result f : result)
    {
        if (!dominate(r, f))
            outresult.push_front(f);

    }
    return outresult;
}

bool no_vectorsfrom_gte(list<Result>& result, Result r)
{
    if (result.empty()) return true; // None to be found
    for (Result f : result)
    {
        if (dominate(f, r))
            return false;
    }
    return true;
}

list<Result> minr(list<Result> front, list<Result> f)
// Join two pareto fronts at min node
{
    list<Result> result;
    Result r;
    //clearresult(result);
    if (front.empty()) return f;

    for (Result vecto : front)
    {
        for (Result v : f)
        {
            r.card = v.card;
                for (int w = 0; w < WORLDSIZE; w++)
            {
                if (vecto.r[w]=='0' )//|| (vecto.r[w] == 'x' &&  v.r[w] == '1')) // 0 x 1 order
                    r.r[w] = vecto.r[w];
                else
                    r.r[w] = v.r[w];
            }
            r.r[WORLDSIZE] = '\0'; // End of string
            result = remove_vectors_lte(result, r);
            if (no_vectorsfrom_gte(result, r))
            {
                result.push_back(r);
            }
        }

    }
    return result;
}

list<Result> maxr(list<Result>& front,list<Result>& f)
{
    bool inserted = false;
    if (0/*front.empty()*/) return f;
    else
    {
        list<Result> result = front; // Deepcopy list elements?
        for (Result fi : f)
        {
            for (Result fr : front)
            {
                if (dominate(fi, fr))
                {
                    result.remove(fr); // Modify in result list, not loop ix
                    result.insert(result.begin(),fi);
                    inserted = true;
                }
            }
        }
        if (!inserted) // Insert if not dominated and not equal
        {
            for (Result fi : f)
            {
                bool quit = false;
                for (Result fr : front)
                {
                    if (dominate(fr, fi) || fr == fi)
                    {
                        quit = true;
                        break;
                    }
                }
                if (!quit)
                    result.insert(result.begin(), fi);
            }
        }
        return result;
    }
}

State evaluate_trick(Card move, State& state)
{
    int winner, maxc = 0;
    if (trumpc == NOTRUMP)
    {
        maxc = state.playRank[0], winner = state.playedby[0];
        if (state.playSuit[1] == state.playSuit[0] && state.playRank[1] > maxc)
        {
            maxc = state.playRank[1]; winner = state.playedby[1];
        }
        if (state.playSuit[2] == state.playSuit[0] && state.playRank[2] > maxc)
        {
            maxc = state.playRank[2]; winner = state.playedby[2];
        }
        if (move.suit == state.playSuit[0] && move.rank > maxc)
        {
            maxc = move.rank; winner = move.player;
        }
        
    }
    else // Trump contract
    {
        if (state.playSuit[0] == trumpc || // Someone played a trump
            state.playSuit[1] == trumpc ||
            state.playSuit[2] == trumpc ||
            move.suit == trumpc)
        {
            if (move.suit == trumpc)
            {
                maxc = move.rank; winner = move.player;
            }

            for (int i=0;i<3;i++)
                if (state.playSuit[i] == trumpc && state.playRank[i] > maxc)
                {
                    maxc = state.playRank[i]; winner = state.playedby[i];
                }
        } 
        else // No one played any trump
        {
            maxc = state.playRank[0], winner = state.playedby[0];

            if (state.playSuit[1] == state.playSuit[0] && state.playRank[1] > maxc)
            {
                maxc = state.playRank[1]; winner = state.playedby[1];
            }
            if (state.playSuit[2] == state.playSuit[0] && state.playRank[2] > maxc)
            {
                maxc = state.playRank[2]; winner = state.playedby[2];
            }
            if (move.suit == state.playSuit[0] && move.rank > maxc)
            {
                maxc = move.rank; winner = move.player;
            }
        }
    }
//    printf("%c wins w %c%c\n", "NESW"[winner], "SHDC"[state.playSuit[0]], dcardRank[maxc]);

    if (winner == NORTH || winner == SOUTH)
    {
        state.declarerTricks++;
        state.Min_node = false;
    }
    else
    {
        state.defenseTricks++;
        state.Min_node = true;
    }
    state.firstc = state.handtoplay = winner;

    state.playno = 0;
    for (int i = 0; i < 3; i++)
    {
        state.playSuit[i] = 0;
        state.playRank[i] = 0;
    }
    return state;
}

State playc(Card move, State state)
{
    if (state.playno >= 3) // 4 cards have now been played in the trick
    {
        state.mask[move.player][move.suit] |= (1 << (move.rank));
        state = evaluate_trick(move, state);
    }
    else
    {
        state.playSuit[state.playno] = move.suit;
        state.playRank[state.playno] = move.rank;
        state.playedby[state.playno] = move.player;
        state.mask[move.player][move.suit] |= (1 << (move.rank));
        state.playno++;
        if (state.handtoplay < WEST)
            state.handtoplay++;
        else state.handtoplay = NORTH;

        if (state.Min_node)
            state.Min_node = false;
        else state.Min_node = true;
    }

    return state;
}

void printfc(list<Result>& front)
{
    for (Result r : front)
        printf("%c%c ", "SHDC"[r.card.suit], dcardRank[r.card.rank]);
}

list<Result> au(State state, int M, set<short int>& worlds)
{
    Result result;
    clearresult(result);
//    printf("au(%d)", M);

    if (stopc(state, M, worlds, result))
    {
       list<Result> res;
       res.push_back(result);
       return res;
    }
    map<struct Card, set<short int>> playcards;
    map<struct Card, set<short int>>::iterator it_playcards;

    struct Card card;
    list<Result> front;
    list<Result> f;
    if (state.Min_node)
    {
        set<Playable> allMoves;
        for (int w : worlds)
        {
            set<Playable> l;
            l.merge(legalMoves(w, state));
            allMoves.merge(l);
        }
        for (Playable move : allMoves)
        {
            card.suit = move.suit;
            card.rank = move.rank;
            card.player = move.player;
            playcards[card].insert(move.world);
        }
        for (it_playcards = playcards.begin(); it_playcards != playcards.end(); it_playcards++)
        {
//            showcard(it_playcards->first.suit, it_playcards->first.rank);
            State s = playc(it_playcards->first, state);
            f = au(s, M, it_playcards->second);
//            printresults(f, "f min");
//            printresults(front, "front before min");
            for (Result fc : f)
            {
                fc.card = it_playcards->first; // Store card resulting in this
            }
            front = minr(front, f); 
//            printresults(front, "front after min");
        }
    }
    else // Max node
    {
        set<Playable> allMoves;
        for (int w : worlds)
        {
            set<Playable> l;
            l.merge(legalMoves(w, state));
            allMoves.merge(l);
        }
        int len = 0;
        for (Playable move : allMoves)
        {
            card.suit = move.suit;
            card.rank = move.rank;
            card.player = move.player;
            playcards[card].insert(move.world); // List of worlds w playable card
            len++;
        }
/*        Playable* crds = new Playable[len];
        list<Result>* f3 = new list<Result>[len];
#pragma omp parallel for
        for (int i = 0; i < len; i++)
        {
            State s = playc(crds[i].it_playcards->first, state);
        } */

        for(it_playcards = playcards.begin(); it_playcards != playcards.end(); it_playcards++)
        {
            //            showcard(it_playcards->first.suit, it_playcards->first.rank);
            State s = playc(it_playcards->first, state);
            f = au(s, M - 1, it_playcards->second);
            for (Result fc : f)
            {
                fc.card = it_playcards->first; // Store card resulting in this
            }
            //            printresults(f, "f max");
            //          printresults(front, "front before max");
            front = maxr(front, f);
            //          printresults(front, "front aftermax");*/
        }
//        front = maxr(front, f);

        /*        struct Flist
                {
                    void operator()() {}
                    list<Result> fl;
                };
                Flist flist = std::for_each(
                        std::execution::par,
                        playcards.begin(),
                        playcards.end(),
                        [&](auto&& item)
                        {
                            State s = playc(item->first, state);
                        //    f2->item = au(s, M - 1, item->second);
                        }

                    );
                    //for (it_playcards = playcards.begin(); it_playcards != playcards.end(); it_playcards++)
                      //  front = maxr(front, f2->it_playcards);


            }
        //  printresults(front, "front exit_au"); */
    }
    if (M >= (M0))
        printfc(front);
    return front;
    
}

State start; // Zero-start

double score(list<Result> r)
{
    int ok = 0, nvec=0;
    for (Result ri : r)
    {
        for (int i = 0; i < WORLDSIZE; i++)
            if (ri.r[i] == '1')
                ok++;
        nvec++;
    }
    return ((100.0 * ok) / (WORLDSIZE * nvec));
}

int main()
{
    set<short int> w;
    list<Result> r;
    auto start_time = high_resolution_clock::now();

    State state = start;
    state.firstc = SOUTH; 
    state.handtoplay = SOUTH;
    init();
    for (int i = 0; i < WORLDSIZE; i++)
        w.insert(i);
    M0 = 1;
    r = au(state, M0,w);
    printresults(r,"Final");
    printf(" Score: %5.2f %%. ",score(r));
    auto stop_time = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop_time - start_time);
    printf("Time taken by function : %f seconds\n", duration.count()/1000000.0);

    return 0;
}

