/* OSCATS: Open-Source Computerized Adaptive Testing System
 * Three-Parameter Logistic IRT Model
 * Copyright 2010 Michael Culbertson <culbert1@illinois.edu>
 *
 *  OSCATS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OSCATS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OSCATS.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:l3p
 * @title:OscatsContModelL3p
 * @short_description: Three-Parameter Logistic (3PL) Model
 */

#include <math.h>
#include <gsl/gsl_vector.h>
#include "models/l3p.h"

G_DEFINE_TYPE(OscatsContModelL3p, oscats_cont_model_l3p, OSCATS_TYPE_CONT_MODEL);

enum
{
  PARAM_B,
  PARAM_C,
  PARAM_A_FIRST,
};

static void model_constructed (GObject *object);
static guint8 get_max (const OscatsContModel *model);
static gdouble P(const OscatsContModel *model, guint resp, const GGslVector *theta, const OscatsCovariates *covariates);
static gdouble distance(const OscatsContModel *model, const GGslVector *theta, const OscatsCovariates *covariates);
static void logLik_dtheta(const OscatsContModel *model,
                          guint resp, const GGslVector *theta,
                          const OscatsCovariates *covariates,
                          GGslVector *grad, GGslMatrix *hes, gboolean I);
static void logLik_dparam(const OscatsContModel *model,
                          guint resp, const GGslVector *theta,
                          const OscatsCovariates *covariates,
                          GGslVector *grad, GGslMatrix *hes);

static void oscats_cont_model_l3p_class_init (OscatsContModelL3pClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  OscatsContModelClass *model_class = OSCATS_CONT_MODEL_CLASS(klass);

  gobject_class->constructed = model_constructed;

  model_class->get_max = get_max;
  model_class->P = P;
  model_class->distance = distance;
  model_class->logLik_dtheta = logLik_dtheta;
  model_class->logLik_dparam = logLik_dparam;
  
}

static void oscats_cont_model_l3p_init (OscatsContModelL3p *self)
{
}

static void model_constructed(GObject *object)
{
  GString *str;
  guint i;
  OscatsContModel *model = OSCATS_CONT_MODEL(object);
  G_OBJECT_CLASS(oscats_cont_model_l3p_parent_class)->constructed(object);

  model->Np = model->Ndims + PARAM_A_FIRST + model->Ncov;
  model->params = g_new0(gdouble, model->Np);

  str = g_string_sized_new(10);
  model->names = g_new(GQuark, model->Np);
  model->names[PARAM_B] = g_quark_from_string("Diff");
  model->names[PARAM_C] = g_quark_from_string("Guess");
  if (model->Ndims < 3)
  {
    g_string_printf(str, "Discr.%d", model->dim1+1);
    model->names[PARAM_A_FIRST] = g_quark_from_string(str->str);
    if (model->Ndims == 2)
    {
      g_string_printf(str, "Discr.%d", model->dim2+1);
      model->names[PARAM_A_FIRST+1] = g_quark_from_string(str->str);
    }
  } else
    for (i=0; i < model->Ndims; i++)
    {
      g_string_printf(str, "Discr.%d", model->dims[i]+1);
      model->names[PARAM_A_FIRST+i] = g_quark_from_string(str->str);
    }
  
  if (model->Ncov > 0)
  {
    model->covariates = model->names + (PARAM_A_FIRST + model->Ndims);
    for (i=0; i < model->Ncov; i++)
    {
      g_string_printf(str, "Cov.%d", i+1);
      model->covariates[i] = g_quark_from_string(str->str);
    }
  }
  g_string_free(str, TRUE);
}

static guint8 get_max (const OscatsContModel *model)
{
  return 1;
}

static gdouble P_star(const OscatsContModel *model, const GGslVector *theta,
                      const OscatsCovariates *covariates)
{
  guint i;
  gdouble z = 0;
  switch (model->Ndims)
  {
    case 2:
      z = -model->params[PARAM_A_FIRST+1] * gsl_vector_get(theta->v, model->dim2);
    case 1:
      z -= model->params[PARAM_A_FIRST] * gsl_vector_get(theta->v, model->dim1);
      break;
    
    default:
      for (i=0; i < model->Ndims; i++)
        z -= model->params[i+PARAM_A_FIRST] * gsl_vector_get(theta->v, model->dims[i]);
  }
  for (i=PARAM_A_FIRST+model->Ndims; i < model->Np; i++)
    z -= oscats_covariates_get(covariates, model->names[i]) * model->params[i];
  z += model->params[PARAM_B];
  return 1/(1+exp(z));
}

static gdouble P(const OscatsContModel *model, guint resp,
                 const GGslVector *theta, const OscatsCovariates *covariates)
{
  gdouble x;
  g_return_val_if_fail(resp == 0 || resp == 1, 0);
  x = model->params[PARAM_C] +
      (1-model->params[PARAM_C]) * P_star(model, theta, covariates);
  return (resp ? x : 1-x);
}

static gdouble distance(const OscatsContModel *model, const GGslVector *theta,
                        const OscatsCovariates *covariates)
{
  guint i;
  gdouble z = 0;
  switch (model->Ndims)
  {
    case 2:
      z = model->params[PARAM_A_FIRST+1] * gsl_vector_get(theta->v, model->dim2);
    case 1:
      z += model->params[PARAM_A_FIRST] * gsl_vector_get(theta->v, model->dim1);
      break;
    
    default:
      for (i=0; i < model->Ndims; i++)
        z += model->params[i+PARAM_A_FIRST] * gsl_vector_get(theta->v, model->dims[i]);
  }
  for (i=PARAM_A_FIRST+model->Ndims; i < model->Np; i++)
    z += oscats_covariates_get(covariates, model->names[i]) * model->params[i];
  z -= model->params[PARAM_B];
  return fabs(z);
}

/* Let P* = 2PL equivalent, thus P = c + (1-c)P*.
 * Note: d[P*, theta_i] = a_i P* Q*.
 *       d[P*, theta_i, theta_j] = a_i a_j (-P*^2 Q* + P* Q*^2)
 * d[log(P), theta_i] = a_i (1-c) P* Q* / P
 * d[log(Q), theta_i] = - a_i (1-c) P* Q* / Q
 * d[log(P), theta_i, theta_j] = a_i a_j (1-c) P* Q*
                                  x [P(1-2P*) - (1-c)P*Q*] / P^2
                               = a_i a_j (1-c) P* Q*
                                  x [c Q*^2 - P*^2] / P^2
 * d[log(Q), theta_i, theta_j] = -a_i a_j (1-c)^2 P* Q*^3 / Q^2
 */
static void logLik_dtheta(const OscatsContModel *model,
                          guint resp, const GGslVector *theta,
                          const OscatsCovariates *covariates,
                          GGslVector *grad, GGslMatrix *hes, gboolean Inf)
{
  gsl_vector *grad_v = (grad ? grad->v : NULL);
  gsl_matrix *hes_v = (hes ? hes->v : NULL);
  guint i, j, I, J;
  guint hes_stride = (hes ? hes_v->tda : 0);
  gdouble c, p, p_star, grad_val, hes_val;
  g_return_if_fail(resp == 0 || resp == 1);

  c = model->params[PARAM_C];
  p_star = P_star(model, theta, covariates);
  p = c + (1-c) * p_star;

  if (resp) grad_val = (1-c) * p_star * (1-p_star) / p;
  else      grad_val = (c-1) * p_star * (1-p_star) / (1-p);
  if (resp) hes_val = (1-c) * p_star * (1-p_star) *
                      (c * (1-p_star)*(1-p_star) - p_star * p_star) / (p*p);
  else      hes_val = -(1-c)*(1-c) * p_star *
                      (1-p_star)*(1-p_star)*(1-p_star) / ((1-p)*(1-p));

  if (Inf)
    hes_val *= -(resp ? p : 1-p);

  switch (model->Ndims)
  {
    case 2:
      if (grad)
        grad_v->data[model->dim2 * grad_v->stride] +=
          model->params[PARAM_A_FIRST+1] * grad_val;
      if (hes)
      {
        hes_v->data[model->dim2 * hes_stride + model->dim2] +=
          model->params[PARAM_A_FIRST+1] * model->params[PARAM_A_FIRST+1]
          * hes_val;
        hes_v->data[model->dim1 * hes_stride + model->dim2] +=
          model->params[PARAM_A_FIRST] * model->params[PARAM_A_FIRST+1]
          * hes_val;
        hes_v->data[model->dim2 * hes_stride + model->dim1] +=
          model->params[PARAM_A_FIRST] * model->params[PARAM_A_FIRST+1]
          * hes_val;
      }
    case 1:
      if (grad)
        grad_v->data[model->dim1 * grad_v->stride] +=
          model->params[PARAM_A_FIRST] * grad_val;
      if (hes)
        hes_v->data[model->dim1 * hes_stride + model->dim1] +=
          model->params[PARAM_A_FIRST] * model->params[PARAM_A_FIRST]
          * hes_val;
      break;
    
    default:
      for (i=0; i < model->Ndims; i++)
      {
        double a_i = model->params[i+PARAM_A_FIRST];
        I = model->dims[i];
        if (grad) grad_v->data[I*grad_v->stride] += a_i * grad_val;
        if (hes)
        {
          hes_v->data[I*hes_stride + I] += a_i * a_i * hes_val;
          for (j=i+1; j < model->Ndims; j++)
          {
            double a_j = model->params[j+PARAM_A_FIRST];
            J = model->dims[j];
            hes_v->data[I*hes_stride + J] += a_i * a_j * hes_val;
            hes_v->data[J*hes_stride + I] += a_i * a_j * hes_val;
          }
        }
      }
  }
}

/* Let P* = 2PL equivalent, thus P = c + (1-c)P*.
 * Note: d[P*, b] = - P* Q*
 *       d[Q*, b] =   P* Q*
 *       d[P*, a_i] = theta_i P* Q*
 *       d[Q*, a_i] = - theta_i P* Q*
 
 * d[log(P), c] = (1-P*)/P
 * d[log(Q), c] = (P*-1)/Q
 * d[log(P), c^2] = -(1-P*)^2 / P^2
 * d[log(Q), c^2] = -(1-P*)^2 / Q^2
 * d[log(P), a_i, c] = -theta_i P* Q* / P^2
 * d[log(Q), a_i, c] = 0
 * d[log(P), b, c] = P* Q* / P^2
 * d[log(Q), b, c] = 0

 * d[log(P), b] = -(1-c) P* Q* / P
 * d[log(Q), b] =  (1-c) P* Q* / Q
 * d[log(P), b^2] = (1-c) P* Q* [c Q*^2 - P*^2] / P^2
 * d[log(Q), b^2] = - (1-c)^2 P* Q*^3 / Q^2

 * d[log(P), a_i] = +theta_i (1-c) P* Q* / P
 * d[log(Q), a_i] = -theta_i (1-c) P* Q* / Q
 * d[log(P), a_i, a_j] = theta_i theta_j (1-c) P* Q* [c Q*^2 - P*^2] / P^2
 * d[log(Q), a_i, a_j] = -theta_i theta_j (1-c)^2 P* Q*^3 / Q^2

 * d[log(P), a_i, b] = theta_i (1-c) P* Q* [-c Q*^2 + P*^2] / P^2
 * d[log(Q), a_i, b] = theta_i (1-c)^2 P* Q*^3 / Q^2
 */
static void logLik_dparam(const OscatsContModel *model,
                          guint resp, const GGslVector *theta,
                          const OscatsCovariates *covariates,
                          GGslVector *grad, GGslMatrix *hes)
{
  gsl_vector *grad_v = (grad ? grad->v : NULL);
  gsl_matrix *hes_v = (hes ? hes->v : NULL);
  gdouble p, p_star, c, grad_val, hes_val, hes_c_val;
  gdouble theta_1, theta_2;
  guint i, j, first_cov = model->Np-model->Ncov;
  guint hes_stride = (hes ? hes_v->tda : 0);
  g_return_if_fail(resp == 0 || resp == 1);

  c = model->params[PARAM_C];
  p_star = P_star(model, theta, covariates);
  p = c + (1-c) * p_star;

  if (resp)
  {
    grad_val = (1-c) * p_star * (1-p_star) / p;
    hes_val = grad_val * (c*(1-p_star)*(1-p_star) - p_star*p_star) / p;
    hes_c_val = -p_star * (1-p_star) / (p*p);
  } else {
    grad_val = -(1-c) * p_star * (1-p_star) / (1-p);
    hes_val = grad_val * (1-c) * (1-p_star) * (1-p_star) / (1-p);
    hes_c_val = 0;
  }


  if (grad)
  {
    grad_v->data[PARAM_B*grad_v->stride] += -grad_val;
    grad_v->data[PARAM_C*grad_v->stride] += (resp ? (1-p_star)/p 
                                                  : (p_star-1)/(1-p) );
  }
  if (hes)
  {
    hes_v->data[PARAM_B*hes_stride + PARAM_B] += hes_val;
    hes_v->data[PARAM_C*hes_stride + PARAM_C] += -(1-p_star)*(1-p_star) /
                                                 (resp ? p*p : (1-p)*(1-p));
    hes_v->data[PARAM_B*hes_stride + PARAM_C] += -hes_c_val;
    hes_v->data[PARAM_C*hes_stride + PARAM_B] += -hes_c_val;
  }
  switch (model->Ndims)
  {
    case 2:
      theta_1 = gsl_vector_get(theta->v, model->dim1);
      theta_2 = gsl_vector_get(theta->v, model->dim2);
      if (grad)
        grad_v->data[(PARAM_A_FIRST+1)*grad_v->stride] += theta_2 * grad_val;
      if (hes)
      {
        hes_v->data[(PARAM_A_FIRST+1)*hes_stride + (PARAM_A_FIRST+1)] +=
          theta_2 * theta_2 * hes_val;
        hes_v->data[(PARAM_B)*hes_stride + (PARAM_A_FIRST+1)] +=
          -theta_2 * hes_val;
        hes_v->data[(PARAM_A_FIRST+1)*hes_stride + (PARAM_B)] +=
          -theta_2 * hes_val;
        hes_v->data[(PARAM_A_FIRST+1)*hes_stride + (PARAM_A_FIRST)] +=
          theta_2 * theta_1 * hes_val;
        hes_v->data[(PARAM_A_FIRST)*hes_stride + (PARAM_A_FIRST+1)] +=
          theta_2 * theta_1 * hes_val;
        hes_v->data[(PARAM_A_FIRST+1)*hes_stride + PARAM_C] += 
          theta_2 * hes_c_val;
        hes_v->data[PARAM_C*hes_stride + (PARAM_A_FIRST+1)] +=
          theta_2 * hes_c_val;
        for (i=first_cov; i < model->Np; i++)
        {
          gdouble val = oscats_covariates_get(covariates, model->names[i]);
          hes_v->data[(PARAM_A_FIRST+1)*hes_stride + i] +=
            theta_2 * val * hes_val;
          hes_v->data[i*hes_stride + (PARAM_A_FIRST+1)] +=
            theta_2 * val * hes_val;
        }
      }
    case 1:
      theta_1 = gsl_vector_get(theta->v, model->dim1);
      if (grad)
        grad_v->data[(PARAM_A_FIRST)*grad_v->stride] += theta_1 * grad_val;
      if (hes)
      {
        hes_v->data[(PARAM_A_FIRST)*hes_stride + (PARAM_A_FIRST)] +=
          theta_1 * theta_1 * hes_val;
        hes_v->data[(PARAM_B)*hes_stride + (PARAM_A_FIRST)] +=
          -theta_1 * hes_val;
        hes_v->data[(PARAM_A_FIRST)*hes_stride + (PARAM_B)] +=
          -theta_1 * hes_val;
        hes_v->data[PARAM_A_FIRST*hes_stride + PARAM_C] +=
          theta_1 * hes_c_val;
        hes_v->data[PARAM_C*hes_stride + PARAM_A_FIRST] +=
          theta_1 * hes_c_val;
        for (i=first_cov; i < model->Np; i++)
        {
          gdouble val = oscats_covariates_get(covariates, model->names[i]);
          hes_v->data[PARAM_A_FIRST*hes_stride + i] +=
            theta_1 * val * hes_val;
          hes_v->data[i*hes_stride + PARAM_A_FIRST] +=
            theta_1 * val * hes_val;
        }
      }
      break;
    default :
      for (i=0; i < model->Ndims; i++)
      {
        theta_1 = gsl_vector_get(theta->v, model->dims[i]);
        if (grad)
          grad_v->data[(i+PARAM_A_FIRST)*grad_v->stride] += theta_1 * grad_val;
        if (hes)
        {
          hes_v->data[(i+PARAM_A_FIRST)*hes_stride + (i+PARAM_A_FIRST)] +=
            theta_1 * theta_1 * hes_val;
          hes_v->data[(PARAM_B)*hes_stride + (i+PARAM_A_FIRST)] +=
            -theta_1 * hes_val;
          hes_v->data[(i+PARAM_A_FIRST)*hes_stride + (PARAM_B)] +=
            -theta_1 * hes_val;
          hes_v->data[(PARAM_C)*hes_stride + (i+PARAM_A_FIRST)] +=
            theta_1 * hes_c_val;
          hes_v->data[(i+PARAM_A_FIRST)*hes_stride + (PARAM_C)] +=
            theta_1 * hes_c_val;
          for (j=i+1; j < model->Ndims; j++)
          {
            theta_2 = gsl_vector_get(theta->v, model->dims[j]);
            hes_v->data[(i+PARAM_A_FIRST)*hes_stride + (j+PARAM_A_FIRST)] +=
              theta_2 * theta_1 * hes_val;
            hes_v->data[(j+PARAM_A_FIRST)*hes_stride + (i+PARAM_A_FIRST)] +=
              theta_2 * theta_1 * hes_val;
          }
          for (j=first_cov; j < model->Np; j++)
          {
            gdouble val = oscats_covariates_get(covariates, model->names[j]);
            hes_v->data[(PARAM_A_FIRST+i)*hes_stride + j] +=
              theta_1 * val * hes_val;
            hes_v->data[j*hes_stride + (PARAM_A_FIRST+i)] +=
              theta_1 * val * hes_val;
          }
        }
      }
  }
  for (i=first_cov; i < model->Np; i++)
  {
    theta_1 = oscats_covariates_get(covariates, model->names[i]);
    if (grad_v)
      grad_v->data[i*grad_v->stride] += theta_1 * grad_val;
    if (hes_v)
    {
      hes_v->data[i*hes_stride + i] += theta_1 * theta_1 * hes_val;
      hes_v->data[PARAM_B*hes_stride + i] += -theta_1 * hes_val;
      hes_v->data[i*hes_stride + PARAM_B] += -theta_1 * hes_val;
      hes_v->data[PARAM_C*hes_stride + i] += theta_1 * hes_c_val;
      hes_v->data[i*hes_stride + PARAM_C] += theta_1 * hes_c_val;
      for (j=i+1; j < model->Np; j++)
      {
        theta_2 = oscats_covariates_get(covariates, model->names[j]);
        hes_v->data[i*hes_stride + j] += theta_1 * theta_2 * hes_val;
        hes_v->data[j*hes_stride + i] += theta_1 * theta_2 * hes_val;
      }
    }
  }
}
