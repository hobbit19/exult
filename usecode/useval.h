/*
 *  useval.h - Values used in Usecode interpreter.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef USEVAL_H
#define USEVAL_H    1

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <new>
#include <string>   // STL string
#include <vector>   // STL container

#include "databuf.h"
#include "objs.h"

class Usecode_class_symbol;

/*
 *  A value that we store can be an integer, string, or array.
 */
class Usecode_value {
public:
	enum Val_type {         // The types:
	    int_type = 0,
	    string_type = 1,    // Allocated string.
	    array_type = 2,
	    pointer_type = 3,
	    class_sym_type = 4, // ->Usecode_class_symbol
	    class_obj_type = 5  // An 'array_type' for a class obj.
	};
	using Usecode_vector = std::vector<Usecode_value>;

private:
	Val_type type;      // Type stored here.
    union {
		long intval;
		std::string strval;
		Usecode_vector arrayval;
		Game_object_shared ptrval;
		Usecode_class_symbol *clssym;
		struct {
			Usecode_value *elems;
			short cnt;
		} clsrefval;
	};	// Anonymous union member
	bool undefined;

	template <typename Op>
	Usecode_value& operate(const Usecode_value &v2);

	void destroy() noexcept {
		switch (type) {
		case array_type:
			arrayval.~Usecode_vector();
			break;
		case string_type:
			using std::string;
			strval.~string();
			break;
		case pointer_type:
			ptrval.~Game_object_shared();
			break;
		default:
			break;
		}
	}

public:
	Usecode_value() : type(int_type), intval(0), undefined(true) {}
	Usecode_value(int ival) : type(int_type), intval(ival), undefined(false) {}
	explicit Usecode_value(std::string s) : type(string_type), strval(std::move(s)), undefined(false) {}
	// Create array with 1st element.
	Usecode_value(int size, Usecode_value *elem0)
		: type(array_type), arrayval(size), undefined(false) {
		if (elem0)
			arrayval[0] = *elem0;
	}
	explicit Usecode_value(Game_object *ptr) : type(pointer_type), undefined(false) {
		if (ptr != nullptr) {
			new (&ptrval) Game_object_shared(ptr->shared_from_this());
		} else {
			new (&ptrval) Game_object_shared;
		}
	}
    explicit Usecode_value(Game_object_shared ptr) : type(pointer_type), ptrval(std::move(ptr)), undefined(false) {}
	Usecode_value(Usecode_class_symbol *ptr) : type(class_sym_type),
		undefined(false) {
		clssym = ptr;
	}
	~Usecode_value();
	Usecode_value &operator=(const Usecode_value &v2);
	Usecode_value &operator=(Usecode_value &&v2) noexcept;
	Usecode_value &operator=(std::string str) noexcept {
		if (type == string_type) {
			strval = std::move(str);
		} else {
			destroy();
			type = string_type;
			new (&strval) std::string(std::move(str));
		}
		return *this;
	}
	Usecode_value &operator=(Game_object *ptr) noexcept {
		if (ptr != nullptr) {
			*this = ptr->shared_from_this();
		} else {
			*this = Game_object_shared();
		}
		return *this;
	}
	Usecode_value &operator=(Game_object_shared ptr) noexcept {
		if (type == pointer_type) {
			ptrval = std::move(ptr);
		} else {
			destroy();
			type = pointer_type;
			new (&ptrval) Game_object_shared(std::move(ptr));
		}
		return *this;
	}
	// Copy ctor.
	Usecode_value(const Usecode_value &v2)
		: type(int_type) {
		*this = v2;
	}
	// Move ctor.
	Usecode_value(Usecode_value &&v2) noexcept
		: type(int_type) {
		*this = std::move(v2);
	}

	Usecode_value& operator+=(const Usecode_value &v2);
	Usecode_value& operator-=(const Usecode_value &v2);
	Usecode_value& operator*=(const Usecode_value &v2);
	Usecode_value& operator/=(const Usecode_value &v2);
	Usecode_value& operator%=(const Usecode_value &v2);
	// Comparator.
	void push_back(int);
	bool operator==(const Usecode_value &v2) const;
	bool operator!=(const Usecode_value &v2) const {
		return !(*this == v2);
	}

	Val_type get_type() const {
		return type;
	}
	size_t get_array_size() const {    // Get size of array.
		return (type == array_type) ? arrayval.size() : 0;
	}
	bool is_array() const {
		return (type == array_type);
	}
	bool is_int() const {
		return (type == int_type);
	}
	bool is_ptr() const {
		return (type == pointer_type);
	}
	long get_int_value() const { // Get integer value.
#ifdef DEBUG
		if (type == pointer_type || (type == int_type && (intval > 0x10000 || intval < -0x10000)))
			std::cerr << "Probable attempt at getting int value of pointer!!" << std::endl;
#endif
		return ((type == int_type) ? intval : 0);
	}
	Game_object *get_ptr_value() const { // Get pointer value.
		return ((type == pointer_type) ? ptrval.get() : nullptr);
	}
	// Get string value.
	const char *get_str_value() const {
		static char const *emptystr = "";
		return ((type == string_type) ? strval.c_str() :
		        ((undefined ||
		          (type == array_type && arrayval.empty())) ? emptystr : 0));
	}
	long need_int_value() const {
		// Convert strings.
		const char *str = get_str_value();
		return str ? std::atoi(str)
		       : ((type == array_type && get_array_size())
		          ? arrayval[0].need_int_value()
		          // Pointer = ref.
		          : (type == pointer_type ? (reinterpret_cast<uintptr>(ptrval.get()) & 0x7ffffff)
		             : get_int_value()));
	}
	// Add array element. (No checking!)
	void put_elem(int i, Usecode_value &val) {
		arrayval[i] = val;
	}
	// Get an array element.
	Usecode_value &get_elem(int i) {
		static Usecode_value zval(0);
		return (type == array_type) ? arrayval[i] : zval;
	}
	// Get an array element.
	const Usecode_value &get_elem(int i) const {
		static const Usecode_value zval(0);
		return (type == array_type) ? arrayval[i] : zval;
	}
	Usecode_value &operator[](int i) {
		assert(type == array_type);
		return arrayval[i];
	}
	const Usecode_value &operator[](int i) const {
		assert(type == array_type);
		return arrayval[i];
	}
	// Get array elem. 0, or this.
	Usecode_value &get_elem0() {
		static Usecode_value zval(0);
		return (type == array_type) ? (get_array_size() ? arrayval[0]
		                               : zval) : *this;
	}
	void steal_array(Usecode_value &v2);
	bool is_false() const {  // Represents a FALSE value?
		switch (type) {
		case int_type:
			return intval == 0;
		case pointer_type:
			return ptrval == NULL;
		case array_type:
			return arrayval.empty();
		default:
			return false;
		}
	}
	bool is_true() const {
		return !is_false();
	}

	bool is_undefined() const {
		return undefined;
	}

	int resize(int new_size);   // Resize array.
	// Look in array for given value.
	int find_elem(const Usecode_value &val);
	// Concat. to end of this array.
	Usecode_value &concat(Usecode_value &val2);
	void append(int *vals, int cnt);// Append integer values.
	// Add value(s) to an array.
	int add_values(int index, Usecode_value &val2);
	void print(std::ostream &out, bool shortformat = false); // Print in ASCII.
	// Save/restore.
	bool save(DataSource *out);
	bool restore(DataSource *in);
	// Class objects.
	void class_new(Usecode_class_symbol *cls, int nvars);
	void class_delete();
	Usecode_value &nth_class_var(int n) {
		// Note:  Elem. 0 is the ->class.
		static Usecode_value zval(0);
		return (type == class_obj_type && n + 1 < clsrefval.cnt) ?
		       clsrefval.elems[n + 1] : zval;
	}
	int get_class_var_count() {
		// Note:  Elem. 0 is the ->class.
		return type == class_obj_type ? clsrefval.cnt - 1 : 0;
	}
	Usecode_class_symbol *get_class_ptr() const {
		return (type == class_obj_type) ?
		       clsrefval.elems[0].clssym : 0;
	}
};

inline Usecode_value operator+(Usecode_value v1, const Usecode_value &v2) {
	return v1 += v2;
}
inline Usecode_value operator-(Usecode_value v1, const Usecode_value &v2) {
	return v1 -= v2;
}
inline Usecode_value operator*(Usecode_value v1, const Usecode_value &v2) {
	return v1 *= v2;
}
inline Usecode_value operator/(Usecode_value v1, const Usecode_value &v2) {
	return v1 /= v2;
}
inline Usecode_value operator%(Usecode_value v1, const Usecode_value &v2) {
	return v1 %= v2;
}

std::ostream &operator<<(std::ostream &out, Usecode_value &val);

#endif
