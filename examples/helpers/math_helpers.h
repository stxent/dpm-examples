/*
 * helpers/math_helpers.h
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef HELPERS_MATH_HELPERS_H_
#define HELPERS_MATH_HELPERS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/helpers.h>
#include <math.h>
/*----------------------------------------------------------------------------*/
#define DEG_TO_RAD  0.01745329251994329577f
#define RAD_TO_DEG  57.2957795130823208768f

typedef struct
{
  float x;
  float y;
} Vector2f;

typedef struct
{
  float x;
  float y;
  float z;
} Vector3f;

typedef struct
{
  Vector3f x;
  Vector3f y;
  Vector3f z;
} Matrix3x3f;
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

static inline void mat3x3fMul(const Matrix3x3f *m1, const Matrix3x3f *m2,
    Matrix3x3f *out)
{
  Matrix3x3f m;

  m.x.x = m1->x.x * m2->x.x + m1->x.y * m2->y.x + m1->x.z * m2->z.x;
  m.x.y = m1->x.x * m2->x.y + m1->x.y * m2->y.y + m1->x.z * m2->z.y;
  m.x.z = m1->x.x * m2->x.z + m1->x.y * m2->y.z + m1->x.z * m2->z.z;

  m.y.x = m1->y.x * m2->x.x + m1->y.y * m2->y.x + m1->y.z * m2->z.x;
  m.y.y = m1->y.x * m2->x.y + m1->y.y * m2->y.y + m1->y.z * m2->z.y;
  m.y.z = m1->y.x * m2->x.z + m1->y.y * m2->y.z + m1->y.z * m2->z.z;

  m.z.x = m1->z.x * m2->x.x + m1->z.y * m2->y.x + m1->z.z * m2->z.x;
  m.z.y = m1->z.x * m2->x.y + m1->z.y * m2->y.y + m1->z.z * m2->z.y;
  m.z.z = m1->z.x * m2->x.z + m1->z.y * m2->y.z + m1->z.z * m2->z.z;

  *out = m;
}

static inline void mat3x3fTranspose(const Matrix3x3f *m, Matrix3x3f *out)
{
  float tmp;

  out->x.x = m->x.x;
  out->y.y = m->y.y;
  out->z.z = m->z.z;

  tmp = m->y.x;
  out->y.x = m->x.y;
  out->x.y = tmp;
  tmp = m->z.x;
  out->z.x = m->x.z;
  out->x.z = tmp;
  tmp = m->z.y;
  out->z.y = m->y.z;
  out->y.z = tmp;
}

static inline void mat3x3vec3fMul(const Matrix3x3f *m1, const Vector3f *v2,
    Vector3f *out)
{
  Vector3f v;

  v.x = m1->x.x * v2->x + m1->x.y * v2->y + m1->x.z * v2->z;
  v.y = m1->y.x * v2->x + m1->y.y * v2->y + m1->y.z * v2->z;
  v.z = m1->z.x * v2->x + m1->z.y * v2->y + m1->z.z * v2->z;

  *out = v;
}

static inline float vec2fNorm(const Vector2f *v)
{
  return sqrtf(v->x * v->x + v->y * v->y);
}

static inline void vec2fNormalize(Vector2f *v)
{
  float norm = vec2fNorm(v);

  if (norm != 0.0f)
  {
    norm = 1.0f / norm;

    v->x *= norm;
    v->y *= norm;
  }
}

static inline void vec3fAdd(const Vector3f *v1, const Vector3f *v2,
    Vector3f *out)
{
  out->x = v1->x + v2->x;
  out->y = v1->y + v2->y;
  out->z = v1->z + v2->z;
}

static inline void vec3fCrossProduct(const Vector3f *v1, const Vector3f *v2,
    Vector3f *out)
{
  Vector3f v;

  v.x = v1->y * v2->z - v1->z * v2->y;
  v.y = v1->z * v2->x - v1->x * v2->z;
  v.z = v1->x * v2->y - v1->y * v2->x;

  *out = v;
}

static inline float vec3fDotProduct(const Vector3f *v1, const Vector3f *v2)
{
  return v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
}

static inline void vec3fMul(const Vector3f *v1, float mul, Vector3f *out)
{
  out->x = v1->x * mul;
  out->y = v1->y * mul;
  out->z = v1->z * mul;
}

static inline float vec3fNorm(const Vector3f *v)
{
  return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static inline void vec3fNormalize(Vector3f *v)
{
  float norm = vec3fNorm(v);

  if (norm != 0.0f)
  {
    norm = 1.0f / norm;

    v->x *= norm;
    v->y *= norm;
    v->z *= norm;
  }
}

static inline void vec3fMakeOrthogonal(const Vector3f *v1, const Vector3f *v2,
    Vector3f *out)
{
  // Gram–Schmidt process: out = v2 − (v2 ⋅ v1 / v1 ⋅ v1) * v1

  const float dotV1V1 = vec3fDotProduct(v1, v1);
  const float dotV2V1 = vec3fDotProduct(v2, v1);
  Vector3f v;

  if (dotV1V1 != 0.0f)
  {
    vec3fMul(v1, -dotV2V1 / dotV1V1, &v);
    vec3fAdd(v2, &v, &v);
  }
  else
  {
    v = (Vector3f){v2->x, v2->y, v2->z};
  }

  *out = v;
}

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* HELPERS_MATH_HELPERS_H_ */
