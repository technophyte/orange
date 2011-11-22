"""\
============================
Lasso regression (``lasso``)
============================

.. index:: regression

.. _`Lasso regression. Regression shrinkage and selection via the lasso`:
    http://www-stat.stanford.edu/~tibs/lasso/lasso.pdf


Example ::

    >>> from Orange.regression import lasso
    >>> table = Orange.data.Table("housing")
    >>> c = lasso.LassoRegressionLearner(table)
    >>> linear.print_lasso_regression_model(c)
    
      Variable  Coeff Est  Std Error          p
     Intercept     22.533
          CRIM     -0.049      0.282      0.770      
            ZN      0.106      0.055      0.030     *
         INDUS     -0.111      0.442      0.920      
          CHAS      1.757      0.669      0.180      
           NOX      0.318      0.483      0.680      
            RM      1.643      0.461      0.480      
           AGE      0.062      0.051      0.230      
           DIS      0.627      0.538      0.930      
           RAD      1.260      0.472      0.070     .
           TAX     -0.074      0.027      0.120      
       PTRATIO      1.331      0.464      0.050     .
             B      0.017      0.007      0.080     .
         LSTAT     -0.209      0.323      0.650      
    Signif. codes:  0 *** 0.001 ** 0.01 * 0.05 . 0.1 empty 1


    All variables have non-zero regression coefficients. 
       
    >>> 


.. autoclass:: LassoRegressionLearner
    :members:

.. autoclass:: LassoRegression
    :members:

Utility functions
-----------------

.. autofunction:: center

.. autofunction:: get_bootstrap_sample

.. autofunction:: permute_responses

.. autofunction:: print_lasso_regression_model


"""

import Orange
import numpy

from Orange.regression import base

from Orange.misc import deprecated_members, deprecated_keywords

def center(X):
    """Centers the data, i.e. subtracts the column means.
    Returns the centered data and the mean.

    :param X: the data arry
    :type table: :class:`numpy.array`
    """
    mu = X.mean(axis=0)
    return X - mu, mu

def get_bootstrap_sample(table):
    """Generates boostrap sample from an Orange Example Table
    and stores it in a new :class:`Orange.data.Table` object

    :param table: the original data sample
    :type table: :class:`Orange.data.Table`
    """
    n = len(table)
    bootTable = Orange.data.Table(table.domain)
    for i in range(n):
        id = numpy.random.randint(0,n)
        bootTable.append(table[id])
    return bootTable

def permute_responses(table):
    """ Permutes values of the class (response) variable.
    The independence between independent variables and the response
    is obtained but the distribution of the response variable is kept.

    :param table: the original data sample
    :type table: :class:`Orange.data.Table`
    """
    n = len(table)
    perm = numpy.random.permutation(n)
    permTable = Orange.data.Table(table.domain, table)
    for i, ins in enumerate(table):
        permTable[i].set_class(table[perm[i]].get_class())
    return permTable

class LassoRegressionLearner(base.BaseRegressionLearner):
    """Fits the lasso regression model, i.e. learns the regression parameters
    The class is derived from
    :class:`Orange.regression.base.BaseRegressionLearner`
    which is used for preprocessing the data (continuization and imputation)
    before fitting the regression parameters

    """
    

    def __init__(self, name='lasso regression', t=1, tol=0.001, \
                 n_boot=100, n_perm=100, imputer=None, continuizer=None):
        """
        :param name: name of the linear model, default 'lasso regression'
        :type name: string
        :param t: tuning parameter, upper bound for the L1-norm of the
            regression coefficients
        :type t: float
        :param tol: tolerance parameter, regression coefficients
            (absoulute value) under tol are set to 0,
            default=0.001
        :type tol: float
        :param n_boot: number of bootstrap samples used for non-parametric
            estimation of standard errors
        :type n_boot: int
        :param n_perm: number of permuations used for non-parametric
            estimation of p-values
        :type n_perm: int
        """

        self.name = name
        self.t = t
        self.tol = tol
        self.n_boot = n_boot
        self.n_perm = n_perm
        self.set_imputer(imputer=imputer)
        self.set_continuizer(continuizer=continuizer)
        
        
    def __call__(self, table, weight=None):
        """
        :param table: data instances.
        :type table: :class:`Orange.data.Table`
        :param weight: the weights for instances. Default: None, i.e.
            all data instances are eqaully important in fitting
            the regression parameters
        :type weight: None or list of Orange.data.variable.Continuous
            which stores weights for instances
        
        """  
        # dicrete values are continuized        
        table = self.continuize_table(table)
        # missing values are imputed
        table = self.impute_table(table)

        domain = table.domain
        X, y, w = table.to_numpy()
        n, m = numpy.shape(X)
        X, mu_x = center(X)
        y, coef0 = center(y)

        import scipy.optimize

        # objective function to be minimized
        objective = lambda beta: numpy.linalg.norm(y - numpy.dot(X, beta))
        # initial guess for the regression parameters
        beta_init = numpy.random.random(m)
        # constraints for the regression coefficients
        cnstr = lambda beta: self.t - sum(numpy.abs(beta))
        # optimal solution
        coefficients = scipy.optimize.fmin_cobyla(objective, beta_init,\
                                                       cnstr)

        # set small coefficients to 0
        def set_2_0(c): return c if abs(c) > self.tol else 0
        coefficients = map(set_2_0, coefficients)

        # bootstrap estimator of standard error of the coefficient estimators
        # assumption: fixed t
        if self.n_boot > 0:
            coeff_b = [] # bootstrapped coefficients
            for i in range(self.n_boot):
                tmp_table = get_bootstrap_sample(table)
                l = LassoRegressionLearner(t=self.t, n_boot=0, n_perm=0)
                c = l(tmp_table)
                coeff_b.append(c.coefficients)
            std_errors_fixed_t = numpy.std(coeff_b, axis=0)
        else:
            std_errors_fixed_t = [float("nan")] * m

        # permutation test to obtain the significance of the regression
        #coefficients
        if self.n_perm > 0:
            coeff_p = []
            for i in range(self.n_perm):
                tmp_table = permute_responses(table)
                l = LassoRegressionLearner(t=self.t, n_boot=0, n_perm=0)
                c = l(tmp_table)
                coeff_p.append(c.coefficients)
            p_vals = \
                   numpy.sum(abs(numpy.array(coeff_p))>\
                             abs(numpy.array(coefficients)), \
                             axis=0)/float(self.n_perm)
        else:
            p_vals = [float("nan")] * m

        # dictionary of regression coefficients with standard errors
        # and p-values
        dict_model = {}
        for i, var in enumerate(domain.attributes):
            dict_model[var.name] = (coefficients[i], std_errors_fixed_t[i], p_vals[i])            
       
        return LassoRegression(domain=domain, class_var=domain.class_var,
                               coef0=coef0, coefficients=coefficients,
                               std_errors_fixed_t=std_errors_fixed_t,
                               p_vals=p_vals,
                               dict_model= dict_model,
                               mu_x=mu_x)

deprecated_members({"nBoot": "n_boot",
                    "nPerm": "n_perm"}, 
                   wrap_methods=["__init__"],
                   in_place=True)(LassoRegressionLearner)

class LassoRegression(Orange.classification.Classifier):
    """Lasso regression predicts value of the response variable
    based on the values of independent variables.

    .. attribute:: coef0

        intercept (sample mean of the response variable)    

    .. attribute:: coefficients

        list of regression coefficients. 

    .. attribute:: std_errors_fixed_t

        list of standard errors of the coefficient estimator for the fixed
        tuning parameter t. The standard errors are estimated using
        bootstrapping method.

    .. attribute:: p_vals

        list of p-values for the null hypothesis that the regression
        coefficients equal 0 based on non-parametric permutation test

    .. attribute:: dict_model

        dictionary of statistical properties of the model.
        Keys - names of the independent variables
        Values - tuples (coefficient, standard error, p-value) 

    .. attribute:: mu_x

        the sample mean of the all independent variables    

    """ 
    def __init__(self, domain=None, class_var=None, coef0=None,
                 coefficients=None, std_errors_fixed_t=None, p_vals=None,
                 dict_model=None, mu_x=None):
        self.domain = domain
        self.class_var = class_var
        self.coef0 = coef0
        self.coefficients = coefficients
        self.std_errors_fixed_t = std_errors_fixed_t
        self.p_vals = p_vals
        self.dict_model = dict_model
        self.mu_x = mu_x

    @deprecated_keywords({"resultType": "result_type"})
    def __call__(self, instance, result_type=Orange.core.GetValue):
        """
        :param instance: data instance for which the value of the response
            variable will be predicted
        :type instance: 
        """  
        ins = Orange.data.Instance(self.domain, instance)
        if "?" in ins: # missing value -> corresponding coefficient omitted
            def miss_2_0(x): return x if x != "?" else 0
            ins = map(miss_2_0, ins)
            ins = numpy.array(ins)[:-1] - self.mu_x
        else:
            ins = numpy.array(ins.native())[:-1] - self.mu_x

        y_hat = numpy.dot(self.coefficients, ins) + self.coef0 
        y_hat = self.class_var(y_hat)
        dist = Orange.statistics.distribution.Continuous(self.class_var)
        dist[y_hat] = 1.0
        if result_type == Orange.core.GetValue:
            return y_hat
        if result_type == Orange.core.GetProbabilities:
            return dist
        return (y_hat, dist)    

deprecated_members({"muX": "mu_x",
                    "stdErrorsFixedT": "std_errors_fixed_t",
                    "pVals": "p_vals",
                    "dictModel": "dict_model"},
                   wrap_methods=["__init__"],
                   in_place=True)(LassoRegression)


@deprecated_keywords({"skipZero": "skip_zero"})
def print_lasso_regression_model(lr, skip_zero=True):
    """Pretty-prints Lasso regression model,
    i.e. estimated regression coefficients with standard errors
    and significances. Standard errors are obtained using bootstrapping
    method and significances by the permuation test

    :param lr: a Lasso regression model object.
    :type lr: :class:`LassoRegression`
    :param skip_zero: if True variables with estimated coefficient equal to 0
        are omitted
    :type skip_zero: boolean
    """
    
    from string import join
    m = lr
    labels = ('Variable', 'Coeff Est', 'Std Error', 'p')
    print join(['%10s' % l for l in labels], ' ')

    fmt = "%10s " + join(["%10.3f"]*3, " ") + " %5s"
    fmt1 = "%10s %10.3f"

    def get_star(p):
        if p < 0.001: return  "*"*3
        elif p < 0.01: return "*"*2
        elif p < 0.05: return "*"
        elif p < 0.1: return  "."
        else: return " "

    stars =  get_star(m.p_vals[0])
    print fmt1 % ('Intercept', m.coef0)
    skipped = []
    for i in range(len(m.domain.attributes)):
        if m.coefficients[i] == 0. and skip_zero:
            skipped.append(m.domain.attributes[i].name)
            continue            
        stars = get_star(m.p_vals[i])
        print fmt % (m.domain.attributes[i].name, \
                     m.coefficients[i], m.std_errors_fixed_t[i], \
                     m.p_vals[i], stars)
    print "Signif. codes:  0 *** 0.001 ** 0.01 * 0.05 . 0.1 empty 1"
    print "\n"
    if skip_zero:
        k = len(skipped)
        if k == 0:
            print "All variables have non-zero regression coefficients. "
        else:
            suff = "s" if k > 1 else ""
            print "For %d variable%s the regression coefficient equals 0: " \
                  % (k, suff)
            for var in skipped:
                print var


if __name__ == "__main__":

    import Orange
    
    table = Orange.data.Table("housing.tab")        

    c = LassoRegressionLearner(table, t=len(table.domain))
    print_lasso_regression_model(c)