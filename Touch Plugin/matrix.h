#pragma once

#include "CPlusPlus_Common.h"

#include <iostream>
#include <iomanip>
#include <exception>
#include <limits>


template <class T> class Matrix44
{
public:

	//-------------------
	// Access to elements
	//-------------------

	T           x[4][4];

	T *         operator [] (int i);
	const T *   operator [] (int i) const;

	Matrix44();
	// 1 0 0 0
	// 0 1 0 0
	// 0 0 1 0
	// 0 0 0 1

	Matrix44(T a);
	// a a a a
	// a a a a
	// a a a a
	// a a a a

	Matrix44(const T a[4][4]);
	// a[0][0] a[0][1] a[0][2] a[0][3]
	// a[1][0] a[1][1] a[1][2] a[1][3]
	// a[2][0] a[2][1] a[2][2] a[2][3]
	// a[3][0] a[3][1] a[3][2] a[3][3]

	Matrix44(T a, T b, T c, T d, T e, T f, T g, T h,
		T i, T j, T k, T l, T m, T n, T o, T p);
	// a b c d
	// e f g h
	// i j k l
	// m n o p

	//--------------------------------
	// Copy constructor and assignment
	//--------------------------------

	Matrix44(const Matrix44 &v);
	template <class S> explicit Matrix44(const Matrix44<S> &v);

	const Matrix44 &    operator = (const Matrix44 &v);
	const Matrix44 &    operator = (T a);

	//---------
	// Identity
	//---------

	/*void                makeIdentity();*/


	//---------
	// Equality
	//---------

	bool                operator == (const Matrix44 &v) const;
	bool                operator != (const Matrix44 &v) const;

	//------------------------
	// Component-wise addition
	//------------------------

	const Matrix44 &    operator += (const Matrix44 &v);
	const Matrix44 &    operator += (T a);
	Matrix44            operator + (const Matrix44 &v) const;


	//---------------------------
	// Component-wise subtraction
	//---------------------------

	const Matrix44 &    operator -= (const Matrix44 &v);
	const Matrix44 &    operator -= (T a);
	Matrix44            operator - (const Matrix44 &v) const;


	//------------------------------------
	// Component-wise multiplication by -1
	//------------------------------------

	Matrix44            operator - () const;
	const Matrix44 &    negate();


	//------------------------------
	// Component-wise multiplication
	//------------------------------

	const Matrix44 &    operator *= (T a);
	Matrix44            operator * (T a) const;


	//-----------------------------------
	// Matrix-times-matrix multiplication
	//-----------------------------------

	const Matrix44 &    operator *= (const Matrix44 &v);
	Matrix44            operator * (const Matrix44 &v) const;
	TD::Position			operator * (const TD::Position &v) const;

	static void         multiply(const Matrix44 &a,    // assumes that
		const Matrix44 &b,    // &a != &c and
		Matrix44 &c);         // &b != &c.

	//-----------------------------------
	// Position-times-matrix multiplication
	//-----------------------------------

	void                multPositionMatrix(const TD::Position &src, TD::Position &dst) const;

	//------------------------------------------------------------
	// Inverse matrix: If singExc is false, inverting a singular
	// matrix produces an identity matrix.  If singExc is true,
	// inverting a singular matrix throws a SingMatrixExc.
	//
	// inverse() and invert() invert matrices using determinants;
	// gjInverse() and gjInvert() use the Gauss-Jordan method.
	//
	// inverse() and invert() are significantly faster than
	// gjInverse() and gjInvert(), but the results may be slightly
	// less accurate.
	// 
	//------------------------------------------------------------

	const Matrix44 &    invert(bool singExc = false);

	Matrix44<T>         inverse(bool singExc = false) const;

	const Matrix44 &    gjInvert(bool singExc = false);

	Matrix44<T>         gjInverse(bool singExc = false) const;
};

//--------------
// Stream output
//--------------
template <class T>
std::ostream &  operator << (std::ostream & s, const Matrix44<T> &m);

template <class T>
inline T
abs(T a);


template <class T>
inline T *
Matrix44<T>::operator [] (int i)
{
	return x[i];
}

template <class T>
inline const T *
Matrix44<T>::operator [] (int i) const
{
	return x[i];
}

template <class T>
inline
Matrix44<T>::Matrix44()
{
	memset(x, 0, sizeof(x));
	x[0][0] = 1;
	x[1][1] = 1;
	x[2][2] = 1;
	x[3][3] = 1;
}

template <class T>
inline
Matrix44<T>::Matrix44(T a)
{
	x[0][0] = a;
	x[0][1] = a;
	x[0][2] = a;
	x[0][3] = a;
	x[1][0] = a;
	x[1][1] = a;
	x[1][2] = a;
	x[1][3] = a;
	x[2][0] = a;
	x[2][1] = a;
	x[2][2] = a;
	x[2][3] = a;
	x[3][0] = a;
	x[3][1] = a;
	x[3][2] = a;
	x[3][3] = a;
}

template <class T>
inline
Matrix44<T>::Matrix44(const T a[4][4])
{
	memcpy(x, a, sizeof(x));
}

template <class T>
inline
Matrix44<T>::Matrix44(T a, T b, T c, T d, T e, T f, T g, T h,
	T i, T j, T k, T l, T m, T n, T o, T p)
{
	x[0][0] = a;
	x[0][1] = b;
	x[0][2] = c;
	x[0][3] = d;
	x[1][0] = e;
	x[1][1] = f;
	x[1][2] = g;
	x[1][3] = h;
	x[2][0] = i;
	x[2][1] = j;
	x[2][2] = k;
	x[2][3] = l;
	x[3][0] = m;
	x[3][1] = n;
	x[3][2] = o;
	x[3][3] = p;
}

template <class T>
inline
Matrix44<T>::Matrix44(const Matrix44 &v)
{
	x[0][0] = v.x[0][0];
	x[0][1] = v.x[0][1];
	x[0][2] = v.x[0][2];
	x[0][3] = v.x[0][3];
	x[1][0] = v.x[1][0];
	x[1][1] = v.x[1][1];
	x[1][2] = v.x[1][2];
	x[1][3] = v.x[1][3];
	x[2][0] = v.x[2][0];
	x[2][1] = v.x[2][1];
	x[2][2] = v.x[2][2];
	x[2][3] = v.x[2][3];
	x[3][0] = v.x[3][0];
	x[3][1] = v.x[3][1];
	x[3][2] = v.x[3][2];
	x[3][3] = v.x[3][3];
}

template <class T>
template <class S>
inline
Matrix44<T>::Matrix44(const Matrix44<S> &v)
{
	x[0][0] = T(v.x[0][0]);
	x[0][1] = T(v.x[0][1]);
	x[0][2] = T(v.x[0][2]);
	x[0][3] = T(v.x[0][3]);
	x[1][0] = T(v.x[1][0]);
	x[1][1] = T(v.x[1][1]);
	x[1][2] = T(v.x[1][2]);
	x[1][3] = T(v.x[1][3]);
	x[2][0] = T(v.x[2][0]);
	x[2][1] = T(v.x[2][1]);
	x[2][2] = T(v.x[2][2]);
	x[2][3] = T(v.x[2][3]);
	x[3][0] = T(v.x[3][0]);
	x[3][1] = T(v.x[3][1]);
	x[3][2] = T(v.x[3][2]);
	x[3][3] = T(v.x[3][3]);
}

template <class T>
inline const Matrix44<T> &
Matrix44<T>::operator = (const Matrix44 &v)
{
	x[0][0] = v.x[0][0];
	x[0][1] = v.x[0][1];
	x[0][2] = v.x[0][2];
	x[0][3] = v.x[0][3];
	x[1][0] = v.x[1][0];
	x[1][1] = v.x[1][1];
	x[1][2] = v.x[1][2];
	x[1][3] = v.x[1][3];
	x[2][0] = v.x[2][0];
	x[2][1] = v.x[2][1];
	x[2][2] = v.x[2][2];
	x[2][3] = v.x[2][3];
	x[3][0] = v.x[3][0];
	x[3][1] = v.x[3][1];
	x[3][2] = v.x[3][2];
	x[3][3] = v.x[3][3];
	return *this;
}

template <class T>
inline const Matrix44<T> &
Matrix44<T>::operator = (T a)
{
	x[0][0] = a;
	x[0][1] = a;
	x[0][2] = a;
	x[0][3] = a;
	x[1][0] = a;
	x[1][1] = a;
	x[1][2] = a;
	x[1][3] = a;
	x[2][0] = a;
	x[2][1] = a;
	x[2][2] = a;
	x[2][3] = a;
	x[3][0] = a;
	x[3][1] = a;
	x[3][2] = a;
	x[3][3] = a;
	return *this;
}

template <class T>
bool
Matrix44<T>::operator == (const Matrix44 &v) const
{
	return x[0][0] == v.x[0][0] &&
		x[0][1] == v.x[0][1] &&
		x[0][2] == v.x[0][2] &&
		x[0][3] == v.x[0][3] &&
		x[1][0] == v.x[1][0] &&
		x[1][1] == v.x[1][1] &&
		x[1][2] == v.x[1][2] &&
		x[1][3] == v.x[1][3] &&
		x[2][0] == v.x[2][0] &&
		x[2][1] == v.x[2][1] &&
		x[2][2] == v.x[2][2] &&
		x[2][3] == v.x[2][3] &&
		x[3][0] == v.x[3][0] &&
		x[3][1] == v.x[3][1] &&
		x[3][2] == v.x[3][2] &&
		x[3][3] == v.x[3][3];
}

template <class T>
bool
Matrix44<T>::operator != (const Matrix44 &v) const
{
	return x[0][0] != v.x[0][0] ||
		x[0][1] != v.x[0][1] ||
		x[0][2] != v.x[0][2] ||
		x[0][3] != v.x[0][3] ||
		x[1][0] != v.x[1][0] ||
		x[1][1] != v.x[1][1] ||
		x[1][2] != v.x[1][2] ||
		x[1][3] != v.x[1][3] ||
		x[2][0] != v.x[2][0] ||
		x[2][1] != v.x[2][1] ||
		x[2][2] != v.x[2][2] ||
		x[2][3] != v.x[2][3] ||
		x[3][0] != v.x[3][0] ||
		x[3][1] != v.x[3][1] ||
		x[3][2] != v.x[3][2] ||
		x[3][3] != v.x[3][3];
}

template <class T>
const Matrix44<T> &
Matrix44<T>::operator += (const Matrix44<T> &v)
{
	x[0][0] += v.x[0][0];
	x[0][1] += v.x[0][1];
	x[0][2] += v.x[0][2];
	x[0][3] += v.x[0][3];
	x[1][0] += v.x[1][0];
	x[1][1] += v.x[1][1];
	x[1][2] += v.x[1][2];
	x[1][3] += v.x[1][3];
	x[2][0] += v.x[2][0];
	x[2][1] += v.x[2][1];
	x[2][2] += v.x[2][2];
	x[2][3] += v.x[2][3];
	x[3][0] += v.x[3][0];
	x[3][1] += v.x[3][1];
	x[3][2] += v.x[3][2];
	x[3][3] += v.x[3][3];

	return *this;
}

template <class T>
const Matrix44<T> &
Matrix44<T>::operator += (T a)
{
	x[0][0] += a;
	x[0][1] += a;
	x[0][2] += a;
	x[0][3] += a;
	x[1][0] += a;
	x[1][1] += a;
	x[1][2] += a;
	x[1][3] += a;
	x[2][0] += a;
	x[2][1] += a;
	x[2][2] += a;
	x[2][3] += a;
	x[3][0] += a;
	x[3][1] += a;
	x[3][2] += a;
	x[3][3] += a;

	return *this;
}

template <class T>
Matrix44<T>
Matrix44<T>::operator + (const Matrix44<T> &v) const
{
	return Matrix44(x[0][0] + v.x[0][0],
		x[0][1] + v.x[0][1],
		x[0][2] + v.x[0][2],
		x[0][3] + v.x[0][3],
		x[1][0] + v.x[1][0],
		x[1][1] + v.x[1][1],
		x[1][2] + v.x[1][2],
		x[1][3] + v.x[1][3],
		x[2][0] + v.x[2][0],
		x[2][1] + v.x[2][1],
		x[2][2] + v.x[2][2],
		x[2][3] + v.x[2][3],
		x[3][0] + v.x[3][0],
		x[3][1] + v.x[3][1],
		x[3][2] + v.x[3][2],
		x[3][3] + v.x[3][3]);
}

template <class T>
const Matrix44<T> &
Matrix44<T>::operator -= (const Matrix44<T> &v)
{
	x[0][0] -= v.x[0][0];
	x[0][1] -= v.x[0][1];
	x[0][2] -= v.x[0][2];
	x[0][3] -= v.x[0][3];
	x[1][0] -= v.x[1][0];
	x[1][1] -= v.x[1][1];
	x[1][2] -= v.x[1][2];
	x[1][3] -= v.x[1][3];
	x[2][0] -= v.x[2][0];
	x[2][1] -= v.x[2][1];
	x[2][2] -= v.x[2][2];
	x[2][3] -= v.x[2][3];
	x[3][0] -= v.x[3][0];
	x[3][1] -= v.x[3][1];
	x[3][2] -= v.x[3][2];
	x[3][3] -= v.x[3][3];

	return *this;
}

template <class T>
const Matrix44<T> &
Matrix44<T>::operator -= (T a)
{
	x[0][0] -= a;
	x[0][1] -= a;
	x[0][2] -= a;
	x[0][3] -= a;
	x[1][0] -= a;
	x[1][1] -= a;
	x[1][2] -= a;
	x[1][3] -= a;
	x[2][0] -= a;
	x[2][1] -= a;
	x[2][2] -= a;
	x[2][3] -= a;
	x[3][0] -= a;
	x[3][1] -= a;
	x[3][2] -= a;
	x[3][3] -= a;

	return *this;
}

template <class T>
Matrix44<T>
Matrix44<T>::operator - (const Matrix44<T> &v) const
{
	return Matrix44(x[0][0] - v.x[0][0],
		x[0][1] - v.x[0][1],
		x[0][2] - v.x[0][2],
		x[0][3] - v.x[0][3],
		x[1][0] - v.x[1][0],
		x[1][1] - v.x[1][1],
		x[1][2] - v.x[1][2],
		x[1][3] - v.x[1][3],
		x[2][0] - v.x[2][0],
		x[2][1] - v.x[2][1],
		x[2][2] - v.x[2][2],
		x[2][3] - v.x[2][3],
		x[3][0] - v.x[3][0],
		x[3][1] - v.x[3][1],
		x[3][2] - v.x[3][2],
		x[3][3] - v.x[3][3]);
}

template <class T>
Matrix44<T>
Matrix44<T>::operator - () const
{
	return Matrix44(-x[0][0],
		-x[0][1],
		-x[0][2],
		-x[0][3],
		-x[1][0],
		-x[1][1],
		-x[1][2],
		-x[1][3],
		-x[2][0],
		-x[2][1],
		-x[2][2],
		-x[2][3],
		-x[3][0],
		-x[3][1],
		-x[3][2],
		-x[3][3]);
}

template <class T>
const Matrix44<T> &
Matrix44<T>::negate()
{
	x[0][0] = -x[0][0];
	x[0][1] = -x[0][1];
	x[0][2] = -x[0][2];
	x[0][3] = -x[0][3];
	x[1][0] = -x[1][0];
	x[1][1] = -x[1][1];
	x[1][2] = -x[1][2];
	x[1][3] = -x[1][3];
	x[2][0] = -x[2][0];
	x[2][1] = -x[2][1];
	x[2][2] = -x[2][2];
	x[2][3] = -x[2][3];
	x[3][0] = -x[3][0];
	x[3][1] = -x[3][1];
	x[3][2] = -x[3][2];
	x[3][3] = -x[3][3];

	return *this;
}

template <class T>
const Matrix44<T> &
Matrix44<T>::operator *= (T a)
{
	x[0][0] *= a;
	x[0][1] *= a;
	x[0][2] *= a;
	x[0][3] *= a;
	x[1][0] *= a;
	x[1][1] *= a;
	x[1][2] *= a;
	x[1][3] *= a;
	x[2][0] *= a;
	x[2][1] *= a;
	x[2][2] *= a;
	x[2][3] *= a;
	x[3][0] *= a;
	x[3][1] *= a;
	x[3][2] *= a;
	x[3][3] *= a;

	return *this;
}

template <class T>
Matrix44<T>
Matrix44<T>::operator * (T a) const
{
	return Matrix44(x[0][0] * a,
		x[0][1] * a,
		x[0][2] * a,
		x[0][3] * a,
		x[1][0] * a,
		x[1][1] * a,
		x[1][2] * a,
		x[1][3] * a,
		x[2][0] * a,
		x[2][1] * a,
		x[2][2] * a,
		x[2][3] * a,
		x[3][0] * a,
		x[3][1] * a,
		x[3][2] * a,
		x[3][3] * a);
}

template <class T>
inline Matrix44<T>
operator * (T a, const Matrix44<T> &v)
{
	return v * a;
}

template <class T>
inline const Matrix44<T> &
Matrix44<T>::operator *= (const Matrix44<T> &v)
{
	Matrix44 tmp(T(0));

	multiply(*this, v, tmp);
	*this = tmp;
	return *this;
}

template <class T>
inline TD::Position
Matrix44<T>::operator * (const TD::Position &v) const
{
    TD::Position tmp;

	multPositionMatrix(v, tmp);
	return tmp;
}

template <class T>
inline Matrix44<T>
Matrix44<T>::operator * (const Matrix44<T> &v) const
{
	Matrix44 tmp(T(0));

	multiply(*this, v, tmp);
	return tmp;
}

template <class T>
void
Matrix44<T>::multiply(const Matrix44<T> &a,
	const Matrix44<T> &b,
	Matrix44<T> &c)
{
	const T *ap = &a.x[0][0];
	const T *bp = &b.x[0][0];
	T *cp = &c.x[0][0];

	T a0, a1, a2, a3;

	a0 = ap[0];
	a1 = ap[1];
	a2 = ap[2];
	a3 = ap[3];

	cp[0] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[1] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[2] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[3] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

	a0 = ap[4];
	a1 = ap[5];
	a2 = ap[6];
	a3 = ap[7];

	cp[4] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[5] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[6] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[7] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

	a0 = ap[8];
	a1 = ap[9];
	a2 = ap[10];
	a3 = ap[11];

	cp[8] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[9] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[10] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[11] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

	a0 = ap[12];
	a1 = ap[13];
	a2 = ap[14];
	a3 = ap[15];

	cp[12] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
	cp[13] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
	cp[14] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
	cp[15] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];
}

template <class T>
void
Matrix44<T>::multPositionMatrix(const TD::Position &src, TD::Position &dst) const
{
	T a, b, c, w;

	a = src.x * x[0][0] + src.y * x[0][1] + src.z * x[0][2] + x[0][3];
	b = src.x * x[1][0] + src.y * x[1][1] + src.z * x[1][2] + x[1][3];
	c = src.x * x[2][0] + src.y * x[2][1] + src.z * x[2][2] + x[2][3];
	w = src.x * x[3][0] + src.y * x[3][1] + src.z * x[3][2] + x[3][3];

	dst.x = static_cast<float>(a / w);
	dst.y = static_cast<float>(b / w);
	dst.z = static_cast<float>(c / w);
}

//--------------------------------
// Implementation of stream output
//--------------------------------

template <class T>
std::ostream &
operator << (std::ostream &s, const Matrix44<T> &m)
{
	std::ios_base::fmtflags oldFlags = s.flags();
	int width;

	if (s.flags() & std::ios_base::fixed)
	{
		s.setf(std::ios_base::showpoint);
		width = static_cast<int>(s.precision()) + 5;
	}
	else
	{
		s.setf(std::ios_base::scientific);
		s.setf(std::ios_base::showpoint);
		width = static_cast<int>(s.precision()) + 8;
	}

	s << "(" << std::setw(width) << m[0][0] <<
		" " << std::setw(width) << m[0][1] <<
		" " << std::setw(width) << m[0][2] <<
		" " << std::setw(width) << m[0][3] << "\n" <<

		" " << std::setw(width) << m[1][0] <<
		" " << std::setw(width) << m[1][1] <<
		" " << std::setw(width) << m[1][2] <<
		" " << std::setw(width) << m[1][3] << "\n" <<

		" " << std::setw(width) << m[2][0] <<
		" " << std::setw(width) << m[2][1] <<
		" " << std::setw(width) << m[2][2] <<
		" " << std::setw(width) << m[2][3] << "\n" <<

		" " << std::setw(width) << m[3][0] <<
		" " << std::setw(width) << m[3][1] <<
		" " << std::setw(width) << m[3][2] <<
		" " << std::setw(width) << m[3][3] << ")\n";

	s.flags(oldFlags);
	return s;
}

template <class T>
const Matrix44<T> &
Matrix44<T>::gjInvert(bool singExc)
{
	*this = gjInverse(singExc);
	return *this;
}

template <class T>
Matrix44<T>
Matrix44<T>::gjInverse(bool singExc) const
{
	int i, j, k;
	Matrix44 s;
	Matrix44 t(*this);

	// Forward elimination

	for (i = 0; i < 3; i++)
	{
		int pivot = i;

		T pivotsize = t[i][i];

		if (pivotsize < 0)
			pivotsize = -pivotsize;

		for (j = i + 1; j < 4; j++)
		{
			T tmp = t[j][i];

			if (tmp < 0)
				tmp = -tmp;

			if (tmp > pivotsize)
			{
				pivot = j;
				pivotsize = tmp;
			}
		}

		if (pivotsize == 0)
		{
			if (singExc)
				throw std::runtime_error("Cannot invert singular matrix.");

			return Matrix44();
		}

		if (pivot != i)
		{
			for (j = 0; j < 4; j++)
			{
				T tmp;

				tmp = t[i][j];
				t[i][j] = t[pivot][j];
				t[pivot][j] = tmp;

				tmp = s[i][j];
				s[i][j] = s[pivot][j];
				s[pivot][j] = tmp;
			}
		}

		for (j = i + 1; j < 4; j++)
		{
			T f = t[j][i] / t[i][i];

			for (k = 0; k < 4; k++)
			{
				t[j][k] -= f * t[i][k];
				s[j][k] -= f * s[i][k];
			}
		}
	}

	// Backward substitution

	for (i = 3; i >= 0; --i)
	{
		T f;

		if ((f = t[i][i]) == 0)
		{
			if (singExc)
				throw std::runtime_error("Cannot invert singular matrix.");

			return Matrix44();
		}

		for (j = 0; j < 4; j++)
		{
			t[i][j] /= f;
			s[i][j] /= f;
		}

		for (j = 0; j < i; j++)
		{
			f = t[j][i];

			for (k = 0; k < 4; k++)
			{
				t[j][k] -= f * t[i][k];
				s[j][k] -= f * s[i][k];
			}
		}
	}

	return s;
}

template <class T>
const Matrix44<T> &
Matrix44<T>::invert(bool singExc)
{
	*this = inverse(singExc);
	return *this;
}

template <class T>
Matrix44<T>
Matrix44<T>::inverse(bool singExc) const
{
	if (x[0][3] != 0 || x[1][3] != 0 || x[2][3] != 0 || x[3][3] != 1)
		return gjInverse(singExc);

	Matrix44 s(x[1][1] * x[2][2] - x[2][1] * x[1][2],
		x[2][1] * x[0][2] - x[0][1] * x[2][2],
		x[0][1] * x[1][2] - x[1][1] * x[0][2],
		0,

		x[2][0] * x[1][2] - x[1][0] * x[2][2],
		x[0][0] * x[2][2] - x[2][0] * x[0][2],
		x[1][0] * x[0][2] - x[0][0] * x[1][2],
		0,

		x[1][0] * x[2][1] - x[2][0] * x[1][1],
		x[2][0] * x[0][1] - x[0][0] * x[2][1],
		x[0][0] * x[1][1] - x[1][0] * x[0][1],
		0,

		0,
		0,
		0,
		1);

	T r = x[0][0] * s[0][0] + x[0][1] * s[1][0] + x[0][2] * s[2][0];

	if (abs(r) >= 1)
	{
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				s[i][j] /= r;
			}
		}
	}
	else
	{
		T mr = abs(r) / (std::numeric_limits<int>::min)();

		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				if (mr > abs(s[i][j]))
				{
					s[i][j] /= r;
				}
				else
				{
					if (singExc)
						throw std::runtime_error("Cannot invert singular matrix.");

					return Matrix44();
				}
			}
		}
	}

	s[3][0] = -x[3][0] * s[0][0] - x[3][1] * s[1][0] - x[3][2] * s[2][0];
	s[3][1] = -x[3][0] * s[0][1] - x[3][1] * s[1][1] - x[3][2] * s[2][1];
	s[3][2] = -x[3][0] * s[0][2] - x[3][1] * s[1][2] - x[3][2] * s[2][2];

	return s;
}


template <class T>
inline T
abs(T a)
{
	return (a > T(0)) ? a : -a;
}
