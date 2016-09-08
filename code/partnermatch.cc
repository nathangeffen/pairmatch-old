
/* Code to test partner matching algorithms.
   Copyright (C) Nathan Geffen.
   See LICENSE file for copyright details.
*/

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <functional>
#include <random>
#include <string>
#include <vector>
#include <unordered_map>

#include <boost/lexical_cast.hpp>

#define MALE 0
#define FEMALE 1
#define MIN_AGE 10
#define MAX_AGE 100

#define GRAPH_ACCURACY 10000000

struct Effectiveness {
  unsigned avg_rank = 0;
  double avg_distance = 0.0;
};


class Agent;

/* Parameters for the simulation are encapsulated in this struct. */

struct InitialVals {
  std::mt19937 rng;
  double prob_male = 0.5;
  double prob_hiv_pos = 0.1;
  double prob_tst_pos = 0.4;
  double prob_tb_inf = 0.45;
  double prob_tb_sick = 0.01;
  double prob_hetero = 0.9;
  double current_date = 2015.0;
  double dob_range = 10.0;
  double max_x_coord = 10.0;
  double max_y_coord = 10.0;
  unsigned last_agent = 0;
  bool verbose = false;

  // proportions for distance calc
  double age_factor = 1.0;
  double orientation_factor = 100.0;
  double tightness_factor = 1.0;
  double distance_factor = 0.1;
  double previous_partner_factor = 500.0;
  double attractor_factor = 0.5;
  double rejector_factor = 0.5;
};

/*
  Table needed by Distribution Match
*/

struct Table {
  size_t start;
  size_t entries;
};

/*
  Sort according to distribution, then use distributional knowledge to locate
  partners.
*/

template <class RandomAccessIterator, class GetBucket>
void dist_sort (RandomAccessIterator first, RandomAccessIterator last,
		RandomAccessIterator out,
		const unsigned lo, const unsigned hi,
		GetBucket getBucket)
{
  std::vector<unsigned> D(hi - lo + 1, 0);
  for (auto it = first; it != last; ++it) {
    unsigned bucket = getBucket(*it);
    ++D[bucket - lo];
  }
  for (unsigned j = 1; j < D.size(); ++j)
    D[j] = D[j - 1] + D[j];
  for (auto it = last - 1; it >= first; --it) {
    unsigned j = getBucket(*it) - lo;
    *(out + D[j] - 1) = *it;
    --D[j];
  }
}


#define DOB_DIM 100
#define SEX_DIM 2
#define SEXOR_DIM 2
#define NUM_BUCKETS (DOB_DIM * SEX_DIM * SEXOR_DIM)

#define GET_BUCKET(age, sex, sexor)			\
  (SEX_DIM * sex + SEXOR_DIM * sexor + (age - MIN_AGE))

class Agent {
public:
  InitialVals &initial_vals_;
  Agent(InitialVals &initial_vals) : initial_vals_(initial_vals) {
    std::uniform_int_distribution<int> uni_age(0,(unsigned)
					       initial_vals_.dob_range);
    std::uniform_real_distribution<double> uni;
    std::uniform_real_distribution<double> uni_x(0, initial_vals_.max_x_coord);
    std::uniform_real_distribution<double> uni_y(0, initial_vals_.max_y_coord);
    id = initial_vals_.last_agent++;
    sex = uni(initial_vals_.rng) < initial_vals_.prob_male ? MALE : FEMALE;
    dob = initial_vals_.current_date - 15.0 - uni_age(initial_vals_.rng);
    tightness = uni(initial_vals.rng);
    hiv_pos = uni(initial_vals_.rng) < initial_vals_.prob_hiv_pos ? true : false;
    tst_pos = uni(initial_vals_.rng) < initial_vals_.prob_tst_pos ? true : false;
    tb_inf = uni(initial_vals_.rng) < initial_vals_.prob_tb_inf ? true : false;
    tb_sick = uni(initial_vals_.rng) < initial_vals_.prob_tb_sick ? true : false;
    hetero = uni(initial_vals_.rng) < initial_vals_.prob_hetero ? 1.0 : 0.0;
    x_coord = uni_x(initial_vals_.rng);
    y_coord = uni_y(initial_vals_.rng);
    attractor = uni(initial_vals_.rng);
    rejector = uni(initial_vals_.rng);
  }

  /* Standard euclidean plane distance function. */
  double euclidean_distance(const Agent &a)
  {
    double x_d = (x_coord - a.x_coord);
    double y_d = (x_coord - a.y_coord);
    return std::sqrt(x_d * x_d + y_d * y_d);
  }


#ifdef ATTRACT_REJECT
  double distance(const Agent &a, unsigned partner_count = 0)
  {
    double attraction = initial_vals_.attractor_factor *
      fabs(attractor - a.attractor);
    double rejection = initial_vals_.rejector_factor *
      fabs((rejector - (1.0 - a.rejector)));
    return attraction + rejection;
  }
  double cluster_value()
  {
    return attractor;
  }

#else
  /* This determines the suitability of a partnership between
     two agents. The smaller the value returned the more
     suitable the partnership. This and the cluster function
     are the only domain specific code here.
  */
  double distance(const Agent &a, unsigned partner_count = 0)
  {
    double prev_partner;
    double age_diff;
    double orientation_diff;
    double tightness_diff;
    double distance_diff;

    if (count (partners.begin(), partners.end(), &a) > partner_count)
      prev_partner = initial_vals_.previous_partner_factor;
    else
      prev_partner = 0;

    age_diff = initial_vals_.age_factor * (dob - a.dob);
    if (sex == a.sex)
      orientation_diff = initial_vals_.orientation_factor * (hetero + a.hetero);
    else
      orientation_diff = initial_vals_.orientation_factor *
				       ((1.0 - hetero) + (1.0 - a.hetero));
    tightness_diff = initial_vals_.tightness_factor * (tightness - a.tightness);
    distance_diff = initial_vals_.distance_factor * euclidean_distance(a);

    return fabs(age_diff) + fabs(orientation_diff) +
				       fabs(tightness_diff) + fabs(distance_diff) + prev_partner;
  }


  /* Function used to cluster agents with similar attributes
     close to each other.
  */
  double cluster_value()
  {
    return initial_vals_.age_factor * dob +
      initial_vals_.orientation_factor * hetero +
      initial_vals_.tightness_factor * tightness;
  }
#endif

  unsigned id;
  unsigned sex;
  int dob;
  double tightness;
  double hetero;
  bool hiv_pos;
  bool tst_pos;
  bool tb_inf;
  bool tb_sick;
  double x_coord;
  double y_coord;
  double weight;
  double attractor;
  double rejector;
  std::vector<Agent* > partners;
};

/* Needed by distribution match */
unsigned get_bucket(const Agent* a)
{
  unsigned age = a->initial_vals_.current_date - a->dob;
  return GET_BUCKET(age, a->sex, a->hetero);
}

/* Class that contains simulation management functions
   and the partner matching algorithms that are being
   compared.
*/

class Simulation {
public:
  InitialVals &initial_vals;
  std::vector<Agent *> agents;
  unsigned iteration = 0;
  std::vector<double> distances;
  std::vector<unsigned> positions;


  Simulation(InitialVals &initial_vals_parm) : initial_vals(initial_vals_parm)
  {}

  ~Simulation() {
    for (auto &a : agents) delete a;
  }

  void init_population(std::size_t size)
  {
    for (std::size_t i = 0; i < size; ++i) {
      Agent *a = new Agent(initial_vals);
      agents.push_back(a);
    }
  }

  void
  print_agent_ids(std::vector<Agent *> &agents,
		  std::string delim = " ",
		  std::string after = "\n")
  {
    for (auto &a : agents)
      std::cout << a->id << delim;
    std::cout << after;
  }

  /* Clear memory of previous partnerships. */
  void reset()
  {
    for (auto &a : agents) a->partners.clear();
    iteration = 1;
  }


  /* Rank the suitability of a partnership. Used as the quality
     measure when comparing partner matching algorithms.
  */

  unsigned
  find_partner_rank(Agent *agent, Agent *partner = NULL)
  {
    unsigned position = 0;
    double d;
    if (partner == NULL) {
      partner = agent->partners.back();
      d = agent->distance(*partner, 1);
    } else {
      if (partner == agent->partners.back())
        d = agent->distance(*partner, 1);
      else
        d = agent->distance(*partner, 0);
    }
    for (auto & a : agents) {
      if (a != agent && a != partner) {
	double x = agent->distance(*a);
	if (x < d) ++position;
      }
    }
    return position;
  }

  /* Given two iterators into the agent vector, finds
     the most suitable partner to the element pointed
     to by the from iterator, but before the to iterator.
  */

  std::vector<Agent *>::iterator
  closest_pair_match(std::vector<Agent *>::iterator from,
		     std::vector<Agent *>::iterator to)
  {
    double smallest_val = DBL_MAX;
    std::vector<Agent *>::iterator closest_agent = to;

    for (auto it = from + 1; it != to; ++it) {
      if ( (*it)->partners.size() < iteration) {
	double distance = (*from)->distance(**it);
	if (distance < smallest_val) {
	  smallest_val = distance;
	  closest_agent = it;
	}
      }
    }
    return closest_agent;
  }

  /* Same as above but looks at most min(n, to-from) entries.
   */
  std::vector<Agent *>::iterator
  closest_pair_match_n(std::vector<Agent *>::iterator from,
		       std::vector<Agent *>::iterator to,
		       unsigned n)
  {
    double smallest_val = DBL_MAX;
    std::vector<Agent *>::iterator closest_agent = to;

    unsigned j = 0;
    for (auto it = from + 1; j < n && it != to; ++it) {
      if ( (*it)->partners.size() < iteration) {
	double distance = (*from)->distance(**it);
	if (distance < smallest_val) {
	  smallest_val = distance;
	  closest_agent = it;
	}
	++j;
      }
    }
    return closest_agent;
  }


  /* Write out all partner comparisons to graph file. */

  void make_graph_all_partners(const char *graph)
  {
    FILE *f = fopen(graph, "w");
    FILE *g = fopen("graph.csv", "w");
    unsigned vertices = (unsigned) agents.size();
    unsigned edges = vertices * (vertices - 1) / 2;
    fprintf(f, "%u %d\n", vertices, edges);
    for (unsigned i = 0; i < agents.size(); ++i) {
      for (unsigned j = i + 1; j < agents.size(); ++j) {
        double d = agents[i]->distance(*agents[j], 1);
        fprintf(f, "%d %d %.0f\n", i, j, d * GRAPH_ACCURACY);
        fprintf(g, "%d %d %.7f\n", i, j, d);
      }
    }
    fclose(g);
    fclose(f);
  }

  /* Write out all partners in CSV format */
  void write_partners(const char* outfile)
  {
    FILE *f = fopen(outfile, "w");
    for (auto& a: agents) {
      double d = a->distance(*(a->partners.back()));
      fprintf(f, "%d,%d,%.6f\n", a->id,a->partners.back()->id, d);
    }
    fclose(f);
  }

  /* Reference partner matching algorithm: Brute force.
   */
  void
  brute_force_match()
  {
    std::shuffle(agents.begin(), agents.end(), initial_vals.rng);
    for (auto it = agents.begin(); it != agents.end(); ++it) {
      if ( (*it)->partners.size() < iteration) {
	auto partner =  closest_pair_match(it, agents.end());
	if (partner != agents.end()) {
	  (*it)->partners.push_back(*partner);
	  (*partner)->partners.push_back(*it);
	}
      }
    }
  }

  /* Reference partner matching algorithm: Random match. */

  void random_match()
  {
    std::shuffle(agents.begin(), agents.end(), initial_vals.rng);
    std::uniform_real_distribution<double> uni;
    for (auto it = agents.begin(); it < agents.end() - 1; it+=2) {
      (*it)->partners.push_back( *(it + 1) );
      (* (it + 1) )->partners.push_back(*it);
    }
  }

  /* Random match k algorithm. */

  void random_match_n(unsigned neighbors)
  {
    std::uniform_real_distribution<double> uni;
    std::shuffle(agents.begin(), agents.end(), initial_vals.rng);
    for (auto it = agents.begin(); it < agents.end() - 1; ++it) {
      if ( (*it)->partners.size() < iteration) {
	auto last = agents.end() - it < (neighbors + 1) ?
					agents.end() : it + neighbors + 1;
	auto partner = closest_pair_match_n(it, last, neighbors);
	if (partner != last) {
	  (*it)->partners.push_back(*partner);
	  (*partner)->partners.push_back(*it);
	}
      }
    }
  }


  /* Weighted shuffle algorithm. */
  void weighted_shuffle_match(unsigned neighbors)
  {
    std::uniform_real_distribution<double> uni;
    for (auto & agent: agents) agent->weight =
				 agent->cluster_value() * uni(initial_vals.rng);
    std::sort(agents.rbegin(), agents.rend(),
	      [](Agent *a, Agent *b) {return a->weight < b->weight; });
    for (auto it = agents.begin(); it < agents.end() - 1; ++it) {
      if ( (*it)->partners.size() < iteration) {
	auto last = agents.end() - it < (neighbors + 1) ?
					agents.end() : it + neighbors + 1;
	auto partner = closest_pair_match_n(it, last, neighbors);
	if (partner != last) {
	  (*it)->partners.push_back(*partner);
	  (*partner)->partners.push_back(*it);
	}
      }
    }
  }

  /* Cluster shuffle algorithm. */

  void
  cluster_shuffle_match(unsigned clusters,
			unsigned neighbors)
  {
    unsigned cluster_size = agents.size() / clusters;
    for (auto &a : agents) a->weight = a->cluster_value();
    sort(agents.rbegin(), agents.rend(), [](Agent *a, Agent *b)
	 { return a->weight < b->weight; });
    for (unsigned i = 0; i < clusters; ++i) {
      auto first = agents.begin() + i * cluster_size;
      auto last = first + cluster_size;
      if (last > agents.end()) last = agents.end();
      std::shuffle(first, last, initial_vals.rng);
    }
    for (auto it = agents.begin(); it < agents.end() - 1; ++it) {
      if ( (*it)->partners.size() < iteration) {
	auto last = agents.end() - it < (neighbors + 1) ?
					agents.end() : it + neighbors + 1;
	auto partner = closest_pair_match_n(it, last, neighbors);
	if (partner != last) {
	  (*it)->partners.push_back(*partner);
	  (*partner)->partners.push_back(*it);
	}
      }
    }
  }


  /* Distribution match */

  void
  distribution_match(int ages, unsigned neighbors)
  {
    std::vector<Agent *> unmatched;
    unsigned k = neighbors / (ages + 1);
    std::shuffle(agents.begin(), agents.end(), initial_vals.rng);
    // Make a copy of the agent **POINTERS** - O(n)
    std::vector<Agent *>  copy_agents(agents.size());
    // Sort the agent pointers on age, sex, sexor, desired_age O(n + num buckets)
    dist_sort(agents.begin(), agents.end(), copy_agents.begin(),
	      0, NUM_BUCKETS, get_bucket);
    Table table[NUM_BUCKETS] = {0, 0};

    // Populate the table entries - O(n)
    for(auto & agent: copy_agents)
      ++table[get_bucket(agent)].entries;
    size_t last_index = 0;
    for (size_t i = 0; i < NUM_BUCKETS; ++i) {
      table[i].start = last_index;
      last_index += table[i].entries;
    }

    // Now match - O(n)
    for (auto& agent: agents) {
      // already matched - continue
      if (agent->partners.size() >= iteration) continue;
      Agent *best_partner = NULL;
      double smallest_distance = DBL_MAX;
      size_t best_bucket = NUM_BUCKETS;
      size_t best_last_entry = agents.size();
      size_t best_index = agents.size();
      for (int j = -ages; j < ages; ++j) {
	unsigned pdob = (unsigned) agent->initial_vals_.current_date -
	  agent->dob  + j;
	unsigned psex = (agent->hetero == 1.0) ? (!agent->sex) : agent->sex;
	unsigned psexor = (unsigned) agent->hetero;
	size_t bucket = GET_BUCKET(pdob, psex, psexor);
	auto start_index = table[bucket].start;
	size_t last_entry = table[bucket].start + table[bucket].entries;
	size_t last_index = std::min(std::min(last_entry,
					      start_index + k),
				     agents.size());
	for (size_t i = start_index; i < last_index; ++i) {
	  if (agent == copy_agents[i]) continue;
	  if (copy_agents[i]->partners.size() < iteration) {
	    double distance = copy_agents[i]->distance(*agent);
	    if (distance < smallest_distance) {
	      best_partner = copy_agents[i];
	      smallest_distance = distance;
	      best_bucket = bucket;
	      best_last_entry = last_entry;
	      best_index = i;
	    }
	  }
	}
      }
      if (best_partner) {
	agent->partners.push_back(best_partner);
	best_partner->partners.push_back(agent);
	std::swap(copy_agents[best_index], copy_agents[best_last_entry - 1]);
	--table[best_bucket].entries;
      } else {
        // Can't match, so deal with as exception.
        // This should happen very rarely.
        unmatched.push_back(agent);
      }
    }
    // Deal with the unmatched agents (there should be few of these)
    if (unmatched.size() > 0) {
      for (size_t i = 0; i < unmatched.size() - 1; ++i) {
        if (unmatched[i]->partners.size() < iteration) {
          for (size_t j = unmatched.size() - 1; j > i; --j) {
            if (unmatched[j]->partners.size() < iteration) {
              unmatched[i]->partners.push_back(unmatched[j]);
              unmatched[j]->partners.push_back(unmatched[i]);
              break;
            }
          }
        }
      }
    }
  }

  /* Statistical and timing functions. */

  Effectiveness
  calc_avg_match(bool calc_rank=true, bool calc_distance=true)
  {
    Effectiveness effectiveness;
    unsigned partnerships = 0;
    unsigned samesex = 0;
    unsigned denom = agents.size();
    positions.clear();
    distances.clear();

    for (auto & a: agents) {
      if (a->partners.size() < iteration) {
	printf("Warning unmatched: %d\n", a->id);
	--denom;
      } else {
	assert(a->partners.back());
	assert(a->partners.back()->partners.back());
	assert(a->partners.back()->partners.back() == a);
	++partnerships;
	if (a->sex == a->partners.back()->sex) ++samesex;

        if (calc_distance) {
          double distance = a->distance(*a->partners.back(), 1);
          distances.push_back(distance);
          effectiveness.avg_distance += distance;
        }
        if (calc_rank) {
          unsigned position = find_partner_rank(a);
          positions.push_back(position);
          effectiveness.avg_rank += position;
        }
      }
    }
    if (initial_vals.verbose) {
      std::cout << "Number of partnerships: " << partnerships / 2 << std::endl;
      std::cout << "Number same sex partnerships: " << samesex / 2 << std::endl;
      std::cout << "Number of agents without partners: "
		<< agents.size() - denom << std::endl;
    }
    effectiveness.avg_distance /= denom / 2;
    effectiveness.avg_rank /= denom / 2;
    return effectiveness;
  }
};

/* Time measuring function taken from:
   http://codereview.stackexchange.com/questions/48872/measuring-execution-time-in-c
*/

template<typename TimeT = std::chrono::milliseconds>
struct measure
{
  template<typename F, typename ...Args>
  static typename TimeT::rep execution(F func, Args&&... args)
  {
    auto start = std::chrono::system_clock::now();

    // Now call the function with all the parameters you need.
    func(std::forward<Args>(args)...);

    auto duration = std::chrono::duration_cast< TimeT>
      (std::chrono::system_clock::now() - start);

    return duration.count();
  }
};

template<class InputIterator> double
median (InputIterator  from, InputIterator to, bool sorted = false)
{
  std::size_t l = std::distance(from, to);
  double median;
  if (sorted == false)
    std::sort(from, to);
  if (l % 2 == 0) {
    median = double (from[l/2] + from[l/2 - 1]) / 2.0;
  } else {
    median = from[l/2];
  }
  return median;
}

template<class InputIterator> double
stddev (InputIterator  from, InputIterator to, double mean)
{
  double total = 0.0;

  for (auto it = from; it != to; ++it)
    total += (*it - mean) * (*it - mean);
  auto variance =  (1.0 / ( double (to - from) - 1.0) ) * total;
  return sqrt(variance);
}

template<class InputIterator> std::array<double, 3>
med_iqr(InputIterator  from, InputIterator to, bool sorted = false)
{
  std::array<double, 3> results;
  results[1] = median(from, to, sorted);
  size_t lqr = 0.25 * (to - from);
  results[0] = *(from + lqr);
  size_t hqr = 0.75 * (to - from);
  results[2] = *(from + hqr);
  return results;
}

void
stats(Simulation &s, const char *description,
      std::function<void(void)> func, unsigned iterations = 1, unsigned run = 0,
      bool avg_ranking = true, bool avg_distance = false,
      bool timings_only = false)
{
  Effectiveness effectiveness;

  s.reset();
  for (unsigned i = 0; i < iterations; ++i, ++s.iteration) {
    auto start = std::chrono::system_clock::now();
    func();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>
      (std::chrono::system_clock::now() - start);

    std::cout << run << ", " << description << ", " << i << ", algorithm time, "
	      << duration.count() << std::endl;

    if (timings_only == false) {
      start = std::chrono::system_clock::now();
      effectiveness = s.calc_avg_match(avg_ranking, avg_distance);
      duration = std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now() - start);

      std::cout << run << ", " << description << ", " << i << ", ranker time, "
                << duration.count() << std::endl;

      std::cout << run << ", " << description  << ", " << i << ", mean rank, "
                << effectiveness.avg_rank << std::endl;
      if (avg_ranking) {
        auto statistics = med_iqr(s.positions.begin(), s.positions.end());
        std::cout << run << ", " << description  << ", " << i << ", median rank, "
                  << statistics[1] << std::endl;
        std::cout << run << ", " << description  << ", " << i << ", 25% rank, "
                  << statistics[0] << std::endl;
        std::cout << run << ", " << description  << ", " << i << ", 75% rank, "
                  << statistics[2] << std::endl;
        auto st = stddev(s.positions.begin(), s.positions.end(),
                         effectiveness.avg_rank);
        std::cout << run << ", " << description  << ", " << i << ", stddev rank, "
                  << st << std::endl;

        std::cout << run << ", " << description  << ", " << i
                  << ", mean distance, "
                  << effectiveness.avg_distance << std::endl;
      }
      if (avg_distance) {
        auto statistics = med_iqr(s.distances.begin(), s.distances.end());
        std::cout << run << ", " << description  << ", " << i
                  << ", median distance, "
                  << statistics[1] << std::endl;
        std::cout << run << ", " << description  << ", " << i
                  << ", 25% distance, "
                  << statistics[0] << std::endl;
        std::cout << run << ", " << description  << ", " << i
                  << ", 75% distance, "
                  << statistics[2] << std::endl;
        auto st = stddev(s.distances.begin(), s.distances.end(),
                         effectiveness.avg_distance);
        std::cout << run << ", " << description  << ", " << i << ", stddev rank, "
                  << st << std::endl;
      }
    }
  }
}

//////////////////////////////
/* COMMAND LINE PROCESSING */

/*
  Simple command line processing functions taken from:
  http://stackoverflow.com/questions/865668/parse-command-line-arguments
*/

char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
  char ** itr = std::find(begin, end, option);
  if (itr != end && ++itr != end)
    {
      return *itr;
    }
  return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
  return std::find(begin, end, option) != end;
}

/* END COMMAND LINE PROCESSING */
/////////////////////////////////

void run_tests(std::size_t population = 16,
	       unsigned clusters = 4,
	       int ages = 4,
	       unsigned neighbors = 2,
	       double attractor_factor = 0.5,
	       double rejector_factor = 0.5,
	       unsigned iterations = 1,
	       unsigned seed = 0,
	       unsigned runs = 1,
	       std::string algorithms = std::string("RNWCB"),
               const char *graph = NULL,
               const char *outfile = NULL,
               bool run_blossom = false,
               bool timings_only = false,
	       bool verbose = true)
{
  InitialVals initial_vals;

  initial_vals.verbose = verbose;
  initial_vals.rng.seed(seed);
  initial_vals.attractor_factor = attractor_factor;
  initial_vals.rejector_factor = rejector_factor;

  if (verbose) {
    std::cout << "Population: " << population << std::endl;
    std::cout << "Clusters: " << clusters << std::endl;
    std::cout << "Neighbors: " << neighbors << std::endl;
    std::cout << "Attractor factor: " << attractor_factor << std::endl;
    std::cout << "Rejector factor: " << rejector_factor << std::endl;
  }

  for (unsigned i = 0; i < runs; ++i) {
    Simulation s(initial_vals);
    s.init_population(population);
    if (algorithms.find("R") != std::string::npos)
      stats(s, "Random match", [&](){s.random_match();},
            iterations, i, true, run_blossom, timings_only);
    if (algorithms.find("N") != std::string::npos)
      stats(s, "Random match n", [&](){s.random_match_n(neighbors);},
	    iterations, i, true, run_blossom, timings_only);
    if (algorithms.find("W") != std::string::npos)
      stats(s, "Weighted shuffle", [&](){s.weighted_shuffle_match(neighbors); },
	    iterations, i, true, run_blossom, timings_only);
    if (algorithms.find("C") != std::string::npos)
      stats(s, "Cluster shuffle", [&](){
	  s.cluster_shuffle_match(clusters, neighbors);
	}, iterations, i, true, run_blossom, timings_only);
    if (algorithms.find("D") != std::string::npos)
      stats(s, "Distribution match", [&](){
	  s.distribution_match(ages, neighbors);
	}, iterations, i, true, run_blossom, timings_only);
    if (algorithms.find("B") != std::string::npos)
      stats(s, "Brute force", [&](){s.brute_force_match();},
            iterations, i, true, run_blossom, timings_only);
    if (graph) {
      sort(s.agents.begin(), s.agents.end(), [&](Agent *a, Agent *b)
           {
             return a->id < b->id;
           });
      s.make_graph_all_partners(graph);
      if (run_blossom) {
        std::string command("./blossom5 -e ");
        command += std::string(graph);
        command +=  std::string(" -w output.txt | tail -n 1 | awk '{avg = $3 / ");
        command += boost::lexical_cast<std::string>(GRAPH_ACCURACY);
        command += std::string( " / ");
        command += boost::lexical_cast<std::string>(population / 2);
        command += std::string("; print avg}'");
        printf("%d, Blossom5, %d, mean distance, ", i, 0);
        fflush(stdout);
        if(system(command.c_str()) != 0) {
          std::cerr << "Error executing Blossom5." << std::endl;
          exit(1);
        }
        // Get mean of Blossom5 rank
        FILE *f = fopen("output.txt", "r");
        unsigned from, to;
        s.positions.clear();
        unsigned avg_rank = 0;
        fscanf(f, "%u %u\n", &from, &to);
        for (size_t i = 0; i < population / 2; ++i) {
          fscanf(f, "%u %u\n", &from, &to);
          unsigned r = s.find_partner_rank(s.agents[from],
                                           s.agents[to]);
          s.positions.push_back(r);
          avg_rank += r;
        }
        printf("%d, Blossom5, %d, mean rank, %.2f\n", i, 0,
               (double) avg_rank / (population / 2));
        {
          auto statistics = med_iqr(s.positions.begin(), s.positions.end());
          printf("%d, Blossom5, %d, median rank, %.2f\n", i, 0,
                 statistics[1]);
          printf("%d, Blossom5, %d, 25%% rank, %.2f\n", i, 0,
                 statistics[0]);
          printf("%d, Blossom5, %d, 75%% rank, %.2f\n", i, 0,
                 statistics[2]);
          auto st = stddev(s.positions.begin(), s.positions.end(),
                           (double) avg_rank / (population / 2));
          printf("%d, Blossom5, %d, stddev rank, %.2f\n", i, 0, st);
        }
        fclose(f);
      }
    }
    if (outfile)
      s.write_partners(outfile);
  }
}

int main(int argc, char *argv[])
{
  unsigned population = 16, clusters = 4, neighbors = 2,
    seed = 0, iterations = 1, runs = 1, ages = 5;
  bool verbose = false, run_blossom = false, timings_only = false;
  double attractor_factor = 0.5;
  double rejector_factor = 0.5;

  char *population_str = getCmdOption(argv, argv + argc, "-p");
  char *neighbors_str = getCmdOption(argv, argv + argc, "-n");
  char *clusters_str = getCmdOption(argv, argv + argc, "-c");
  char *ages_str = getCmdOption(argv, argv + argc, "-y");
  char *iterations_str = getCmdOption(argv, argv + argc, "-i");
  char *seed_str = getCmdOption(argv, argv + argc, "-s");
  char *runs_str = getCmdOption(argv, argv + argc, "-r");
  char *alg_str = getCmdOption(argv, argv + argc, "-a");
  char *attractor_str = getCmdOption(argv, argv + argc, "-A");
  char *rejector_str = getCmdOption(argv, argv + argc, "-R");
  char *graph_str = getCmdOption(argv, argv + argc, "-g");
  char *out_str =  getCmdOption(argv, argv + argc, "-o");
  std::string algorithms = "RNWDCB";

  for (int i = 0; i < argc; ++i)
    std::cout << argv[i] << " ";
  std::cout << std::endl;

  if (population_str) {
    population = atoi(population_str);
    if (population > 63) {
      clusters = neighbors = std::round(std::log2(population));
    }
  }
  if (clusters_str)
    clusters = atoi(clusters_str);
  if (ages_str)
    ages = atoi(ages_str);
  if (neighbors_str)
    neighbors = atoi(neighbors_str);
  if (iterations_str)
    iterations = atoi(iterations_str);
  if (seed_str)
    seed = atoi(seed_str);
  if (runs_str)
    runs = atoi(runs_str);
  if (alg_str)
    algorithms = alg_str;
  if (attractor_str)
    attractor_factor = atof(attractor_str);
  if (rejector_str)
    rejector_factor = atof(rejector_str);

  if(cmdOptionExists(argv, argv+argc, "-v"))
    verbose = true;
  if(cmdOptionExists(argv, argv+argc, "-b"))
    run_blossom = true;
  if(cmdOptionExists(argv, argv+argc, "-t"))
    timings_only = true;


#ifdef ATTRACT_REJECT
  printf("Compiled with ATTRACT_REJECT on\n");
#else
  printf("Compiled with ATTRACT_REJECT off\n");
#endif

  run_tests(population, clusters, ages, neighbors,
	    attractor_factor, rejector_factor, iterations,
	    seed, runs, std::string(algorithms), graph_str, out_str,
            run_blossom, timings_only, verbose);
}
