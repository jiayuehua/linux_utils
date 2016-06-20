#ifndef PRIORITYLISTUNORDERSET_H
#define PRIORITYLISTUNORDERSET_H
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/functional/hash.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>
#include <iostream>
#include "elem.h"
#include "memory.h"
template <class T>
bool canErase(const T& t)
{
  return true;
}
template <class T, int BucketsSize>
class PriorityListUnorderSet;

template <class T, int BucketsSize>
std::ostream& operator<<(std::ostream&os, const PriorityListUnorderSet<T, BucketsSize>&cache);

template <class T>
struct PoolType {
  typedef Elem<T> Node;
  typedef typename boost::object_pool<Node, AllocatorMallocFree> type;
};

template<class T , int BucketsSize = 901>
class PriorityListUnorderSet
{
public:
  typedef T key_type;
  typedef Elem<T> value_type;
  typedef  value_type& reference_type;
  typedef boost::object_pool<value_type, AllocatorMallocFree> pool_type;
private:
  //Definition of the intrusive list that will hold Node
  typedef intrusive::member_hook<value_type, intrusive::list_member_hook<> , &value_type::list_hook_> MemberListOption;
  typedef intrusive::list<value_type, MemberListOption> list_t;
  boost::object_pool<value_type, AllocatorMallocFree>& pool_;
  list_t     list_;

  //Definition of the intrusive unordered_set that will hold Node
  typedef intrusive::member_hook
  < value_type, intrusive::unordered_set_member_hook<>
  , &value_type::unordered_set_hook_> MemberUsetOption;
  typedef intrusive::unordered_set
  < value_type, MemberUsetOption> unordered_set_t;
  typename unordered_set_t::bucket_type buckets_[BucketsSize];
  unordered_set_t  unordered_set_;

public:
  //PriorityListUnorderSet(typename PoolType<T>::type& pool): pool_(pool) {}
  typedef typename list_t::const_reverse_iterator const_reverse_iterator;
  typedef typename list_t::reverse_iterator reverse_iterator;
  reference_type back()
  {
    return list_.back();
  }
  reverse_iterator rbegin()
  {
    return list_.rbegin();
  }
  reverse_iterator rend()
  {
    return list_.rend();
  }
  const_reverse_iterator crbegin()const
  {
    return list_.crbegin();
  }
  const_reverse_iterator crend()const
  {
    return list_.crend();
  }
  friend std::ostream& operator<< <>(std::ostream&os, const PriorityListUnorderSet<T, BucketsSize>&);

  PriorityListUnorderSet(pool_type& pool):pool_(pool), unordered_set_(typename unordered_set_t::bucket_traits(buckets_, BucketsSize)) {}

  T* lrufind(T id)
  {
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(value_type(id));

    if (unordered_set_it == unordered_set_.end()) {
      return 0;
    } else {
      typename list_t::iterator l_it = list_.iterator_to(*unordered_set_it);
      list_.erase(l_it);
      list_.push_back(*unordered_set_it);
      return &unordered_set_it->get();
    }
  }

  T* find(T id)
  {
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(value_type(id));

    if (unordered_set_it == unordered_set_.end()) {
      return 0;
    } else {
      return &unordered_set_it->get();
    }
  }

  T* insert(const T& id)
  {
    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(value_type(id));

    if (unordered_set_it != unordered_set_.end()) {
      list_.erase(list_.iterator_to(*unordered_set_it));
      list_.push_back(*unordered_set_it);
      return &unordered_set_it->get();
    } else {
      if (full()) {
        pop_front();
      }

      value_type * pnode = pool_.construct(value_type(id));
      list_.push_back(*pnode);
      unordered_set_.insert(*pnode);
      return &pnode->get();
    }
  }

  void insertInPlace()
  {
    value_type * pnode = pool_.construct();
    list_.push_back(*pnode);
    unordered_set_.insert(*pnode);
  }

  template<class A0>
  void insertInPlace(A0 arg0)
  {
    value_type * pnode = pool_.construct(arg0);
    list_.push_back(*pnode);
    unordered_set_.insert(*pnode);
  }


  template<class A0, class A1>
  void insertInPlace(A0 arg0, A1 arg1)
  {
    value_type * pnode = pool_.construct(arg0, arg1);
    list_.push_back(*pnode);
    unordered_set_.insert(*pnode);
  }

  template<class A0, class A1, class A2>
  void insertInPlace(A0 arg0, A1 arg1, A2 arg2)
  {
    value_type * pnode = pool_.construct(arg0, arg1, arg2);
    list_.push_back(*pnode);
    unordered_set_.insert(*pnode);
  }

  template<class A0, class A1, class A2, class A3>
  void insertInPlace(A0 arg0, A1 arg1, A2 arg2, A3 arg3)
  {
    value_type * pnode = pool_.construct(arg0, arg1, arg2, arg3);
    list_.push_back(*pnode);
    unordered_set_.insert(*pnode);
  }

  bool full()const
  {
    return list_.size() == BucketsSize;
  }
  bool empty()const
  {
    return list_.empty() ;
  }
  size_t size() const
  {
    return list_.size();
  }

  bool erase(const T& id)
  {
    if (list_.empty()) {
      return false;
    }

    typename unordered_set_t::iterator unordered_set_it = unordered_set_.find(value_type(id));

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
  bool pop_back_nodelete()
  {
    if (list_.empty()) {
      return false;
    } else {

      typename list_t::iterator i = list_.end();
      --i;
        value_type* p = & *i;
        unordered_set_.erase(*i);
        list_.erase(i);
        return true;
    }
  }
  bool pop_front()
  {
    if (list_.empty()) {
      return false;
    } else {
      for (typename list_t::iterator i = list_.begin(); i != list_.end(); ++i) {
        if (i->canEraseNow()) {
          value_type* p = & *i;
          unordered_set_.erase(*i);
          list_.erase(i);
          pool_.destroy(p);
          return true;
        }
      }

      typename list_t::iterator i = list_.begin();

      if (i != list_.end()) {
        value_type* p = & *i;
        unordered_set_.erase(*i);
        list_.erase(i);
        pool_.destroy(p);
        return true;
      }

      return false;
    }
  }

  void clear()
  {
    while (pop_front()) {}
  }
  ~PriorityListUnorderSet() {
    clear();
  }
};

template<class T, int BucketsSize>
std::ostream& operator<<(std::ostream&os, const PriorityListUnorderSet<T, BucketsSize>&cache)
{
  typedef Elem<T> ID_Attribute;
  typename PriorityListUnorderSet<T, BucketsSize>::list_t::const_iterator it = cache.list_.begin();
  typename PriorityListUnorderSet<T, BucketsSize>::list_t::const_iterator ite = cache.list_.end();

  for (; it != ite; ++it) {
    std::cout << *it;
  }

  return os;
}
template<class T >
class OutputList
{
public:

  typedef Elem<T> value_type;
private:
  //Definition of the intrusive list that will hold Node
  typedef intrusive::member_hook<value_type, intrusive::list_member_hook<> , &value_type::list_hook_> MemberListOption;
  typedef intrusive::list<value_type, MemberListOption> list_t;
  boost::object_pool<value_type, AllocatorMallocFree>& pool_;
  list_t     list_;

public:
  OutputList(typename PoolType<T>::type& pool):pool_(pool) {}
  typedef typename list_t::const_reverse_iterator const_reverse_iterator;
  const_reverse_iterator crend()const
  {
    return list_.crend();
  }
  const_reverse_iterator crbegin()const
  {
    return list_.crbegin();
  }

  void insert(const T& id)
  {
    value_type * pnode = pool_.construct(value_type(id));
    list_.push_back(*pnode);
  }
  void push_back(value_type& node)
  {
    list_.push_back(node);
  }
  bool empty()
  {
    return list_.empty();
  }

  bool clear()
  {
    typename list_t::iterator j ;
    for (typename list_t::iterator i = list_.begin(); i != list_.end(); i = j) {
      value_type* p = & *i;
      j = ++i;
      list_.erase(--i);
      pool_.destroy(p);
    }
  }

  ~OutputList() {
    clear();
  }
};
#endif