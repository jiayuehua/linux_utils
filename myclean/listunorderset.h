#ifndef LISTUNORDERSET_H
#define LISTUNORDERSET_H
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/functional/hash.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>
#include <iostream>
#include "elem.h"
#include "memory.h"
template <class T, int BucketsSize>
class ListUnorderSet;
template <class T, int BucketsSize>
std::ostream& operator<<(std::ostream&os, const ListUnorderSet<T, BucketsSize>&cache);

template<class T , int BucketsSize = 901>
class ListUnorderSet
{
public:
  typedef T key_type;

private:
  //Definition of the intrusive list that will hold Node
  typedef Elem<T> Node;
  boost::object_pool<Node, AllocatorMallocFree> pool_;
  typedef intrusive::member_hook<Node, intrusive::list_member_hook<>
  , &Node::list_hook_> MemberListOption;
  //Definition of the intrusive unordered_set that will hold Node
  typedef intrusive::member_hook
  < Node, intrusive::unordered_set_member_hook<>
  , &Node::unordered_set_hook_> MemberUsetOption;
  typedef intrusive::unordered_set
  < Node, MemberUsetOption> unordered_set_t;
public:
  friend std::ostream& operator<< <>(std::ostream&os, const ListUnorderSet<T, BucketsSize>&);
  typedef intrusive::list<Node, MemberListOption> list_t;

  ListUnorderSet(): unordered_set_(typename unordered_set_t::bucket_traits(buckets_, BucketsSize)) {}
  T* find(T id)
  {
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(Node(id));

    if (unordered_set_it == unordered_set_.end()) {
      return 0;
    } else {
      typename list_t::iterator l_it = list_.iterator_to(*unordered_set_it);
      list_.erase(l_it);
      list_.push_back(*unordered_set_it);
      return &unordered_set_it->get();
    }
  }

  T* insert(const T& id)
  {
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(Node(id));

    if (unordered_set_it != unordered_set_.end()) {
      list_.erase(list_.iterator_to(*unordered_set_it));
      list_.push_back(*unordered_set_it);
      return &unordered_set_it->get();
    } else {
      if (full())
      {
        pop_front();
      }
      Node * pnode = pool_.construct(Node(id));
      list_.push_back(*pnode);
      unordered_set_.insert(*pnode);
      return &pnode->get();
    }
  }

  bool full()const
  {
    return list_.size() == BucketsSize;
  }

  bool erase(const T& id)
  {
    if (list_.empty()) {
      return false;
    }

    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(Node(id));

    if (unordered_set_it != unordered_set_.end()) {
      unordered_set_.erase(*unordered_set_it);
      list_.erase(list_.iterator_to(*unordered_set_it));
      pool_.destroy(& (*unordered_set_it));
      return true;
    } else {
      return false;
    }
  }

  bool erase_n(int n)
  {
    if (n > 0) {
      for (int i = 0; i < n; ++i) {
        if (!pop_front()) {
          return false;
        }
      }
    }

    return true;
  }

  bool pop_front()
  {
    if (list_.empty()) {
      return false;
    } else {
      Node* p = & list_.front();
      unordered_set_.erase(list_.front());
      list_.erase(list_.begin());
      pool_.destroy(p);
      return true;
    }
  }

  void clear()
  {
    while (pop_front()) {}
  }
  ~ListUnorderSet() {clear();}
private:

  list_t     list_;
  typename unordered_set_t::bucket_type buckets_[BucketsSize];
  unordered_set_t  unordered_set_;

};

template<class T, int BucketsSize>
std::ostream& operator<<(std::ostream&os, const ListUnorderSet<T, BucketsSize>&cache)
{
  typename ListUnorderSet<T, BucketsSize>::list_t::const_iterator it = cache.list_.begin();
  typename ListUnorderSet<T, BucketsSize>::list_t::const_iterator ite = cache.list_.end();

  for (; it != ite; ++it) {
    std::cout << *it;
  }

  return os;
}
#endif
