/*
    This file is part of Orange.

    Orange is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Orange is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Orange; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Authors: Janez Demsar, Blaz Zupan, 1996--2002
    Contact: janez.demsar@fri.uni-lj.si
*/


#include <queue>
#include "stladdon.hpp"

#include "random.hpp"

#include "vars.hpp"
#include "domain.hpp"
#include "examples.hpp"
#include "table.hpp"

#include "contingency.hpp"
#include "discretize.hpp"
#include "classfromvar.hpp"
#include "lookup.hpp"
#include "induce.hpp"
#include "measures.hpp"

#include "redundancy.ppp"


TRemoveRedundant::TRemoveRedundant(bool akeepValues)
: keepValues(akeepValues)
{}


TRemoveRedundantByInduction::TRemoveRedundantByInduction(bool akeepValues)
: TRemoveRedundant(akeepValues)
{}


class T_IntMeasure {
public:
  int attrNo;
  float measure;
  T_IntMeasure(const int &i, const float &m) 
  : attrNo(i), 
    measure(m)
  {};

  inline bool operator < (const T_IntMeasure &o) const
  { return measure>o.measure; }
};


PDomain TRemoveRedundantByInduction::operator()(PExampleGenerator gen, PVarList suspicious, PExampleGenerator *NRGen, int weightID)
{ TExampleTable *newGen = NULL;
  try {
    if (measure->needs==TMeasureAttribute::Generator) {
      newGen=mlnew TExampleTable(gen);

      TVarList candidates;
      if (suspicious && suspicious->size()) {
        PITERATE(TVarList, si, suspicious)
          if (exists(newGen->domain->attributes.getReference(), *si))
            candidates.push_back(*si);
          else
            PITERATE(TVarList, ni, newGen->domain->attributes)
              if (   (*ni)->getValueFrom
                  && (*ni)->getValueFrom.is_derived_from(TClassifierFromVar)
                  && (*ni)->getValueFrom.AS(TClassifierFromVar)->whichVar==*si)
                candidates.push_back(*ni);
      }
      else 
        candidates = newGen->domain->attributes.getReference();

      for(bool doMore = true; doMore; ) {
        priority_queue<T_IntMeasure> measurements;
        int ano=0;
        for(TVarList::iterator vi(newGen->domain->attributes->begin()), ve(newGen->domain->attributes->end());
            vi!=ve;
            vi++, ano++)
          if (find(candidates.begin(), candidates.end(), *vi)!=candidates.end()) {
            T_IntMeasure meas(ano, measure->operator ()(ano, gen, PDistribution(), weightID));
            measurements.push(meas);
          }

        for(doMore = false; !doMore && !measurements.empty(); measurements.pop()) {
          PVariable attr = newGen->domain->attributes->at(measurements.top().attrNo);
          PVariable newVar;

          if (attr->noOfValues()==1)
            newVar=attr;
          else {
            TVarList boundSet;
            boundSet.push_back(attr);
            float foo;
            newVar = featureReducer->operator()(PExampleGenerator(*newGen), boundSet, attr->name+"_r", foo);
          }

          candidates.erase(remove(candidates.begin(), candidates.end(), attr), candidates.end());

          if (   newVar
              && (   (newVar->noOfValues()==1)
                  || (!keepValues && (newVar->noOfValues() < attr->noOfValues())))) {
            PDomain newDomain = CLONE(TDomain, newGen->domain);
            newDomain->delVariable(attr);
            if (newVar->noOfValues()>1)
              newDomain->addVariable(newVar);
            TExampleTable *newNewGen = mlnew TExampleTable(newDomain, PExampleGenerator(*newGen));
            mldelete newGen;
            newGen = newNewGen;
            doMore = true;
          }
        }
      }

      PDomain retDomain = newGen->domain;
      if (NRGen)
        *NRGen = PExampleGenerator(newGen);
      else {
        mldelete newGen;
        newGen = NULL;
      }
      return retDomain;
    }

    else if (measure->needs==TMeasureAttribute::DomainContingency) {
      raiseError("redundancy removal by attribute measure that needs domain contingency is not implemented yet");
    }

    else {// measure->needs==Contingency_Class
      newGen = mlnew TExampleTable(gen);
      priority_queue<T_IntMeasure> measurements;

      { TDomainContingency cont = TDomainContingency(PExampleGenerator(*newGen), weightID);
        int ano = 0;
        for(TDomainContingency::iterator ci(cont.begin()), ei(cont.end()); ci!=ei; ci++, ano++)
          if (   ((*ci)->outerVariable->varType==TValue::INTVAR)
              && (   !suspicious
                  || !suspicious->size()
                  || exists(suspicious.getReference(), gen->domain->attributes->at(ano))))
            measurements.push(T_IntMeasure(ano, measure->operator ()(*ci, cont.classes)));
      }

      while(!measurements.empty()) {
        int attrNo = measurements.top().attrNo;
        PVariable attr = gen->domain->attributes->at(attrNo);
        TVarList boundSet;
        boundSet.push_back(attr);
        float foo;
        PVariable newVar(featureReducer->operator()(PExampleGenerator(*newGen), boundSet, attr->name+"_r", foo));

        if (   newVar
            && (   (newVar->noOfValues()==1)
                || (!keepValues && (newVar->noOfValues() < attr->noOfValues())))) {
          PDomain newDomain = CLONE(TDomain, newGen->domain);
          newDomain->delVariable(attr);
          if (newVar->noOfValues()>1)
            newDomain->addVariable(newVar);
          TExampleTable *newNewGen = mlnew TExampleTable(newDomain, PExampleGenerator(*newGen));
          mldelete newGen;
          newGen = newNewGen;
        }
        measurements.pop();
      }

      PDomain retDomain = newGen->domain;
      if (NRGen) 
        *NRGen = PExampleGenerator(newGen);
      else
        mldelete newGen;
      return retDomain;
    }
  }
  catch (exception) {
    mldelete newGen;
    throw;
  }

  throw 0;
}



TRemoveRedundantByQuality::TRemoveRedundantByQuality(bool aremeasure)
: TRemoveRedundant(false), 
  remeasure(aremeasure)
{}


PDomain TRemoveRedundantByQuality::operator()
  (PExampleGenerator gen, PVarList suspicious, PExampleGenerator *NRGen, int weightID)
{
  if (!remeasure || (measure->needs==TMeasureAttribute::Contingency_Class)) {
    priority_queue<T_IntMeasure> measurements;

    if (measure->needs==TMeasureAttribute::Generator) {
      int ano = 0;
      for(TVarList::iterator vi(gen->domain->attributes->begin()), ve(gen->domain->attributes->end());
          vi!=ve;
          vi++, ano++)
        if (   (   !suspicious
                || !suspicious->size()
                || (find(suspicious->begin(), suspicious->end(), *vi)!=suspicious->end()))) {
          T_IntMeasure meas(ano, measure->operator ()(ano, gen, PDistribution(), weightID));
          measurements.push(meas);
        }
    }
    else {
      TDomainContingency cont(gen, weightID);
      int ano = 0;
      for(TDomainContingency::iterator ci(cont.begin()), ei(cont.end()); ci!=ei; ci++, ano++)
        if (   ((*ci)->outerVariable->varType==TValue::INTVAR)
            && (   !suspicious
                || !suspicious->size()
                || exists(suspicious.getReference(), gen->domain->attributes->at(ano)))) {
          T_IntMeasure meas(ano, measure->operator ()(*ci, cont.classes));
          measurements.push(meas);
        }
    }

    PDomain newDomain = CLONE(TDomain, gen->domain);
    while(   !measurements.empty()
          && (   (measurements.top().measure<=minQuality)
              || (int(newDomain->attributes->size())>removeBut))) {
      newDomain->delVariable(gen->domain->attributes->at(measurements.top().attrNo));
      measurements.pop();
    }

    if (NRGen)
      *NRGen = mlnew TExampleTable(newDomain, gen);

    return newDomain;
  }

  else if (measure->needs==TMeasureAttribute::DomainContingency) {
    raiseError("redundancy removal by attribute measure that needs domain contingency is not implemented yet");
  }

  else /* if (measure->needs==TMeasureAttribute::Generator) */ {
    TExampleTable *newGen = mlnew TExampleTable(gen);
    PDomain retDomain;
    try {

      TSimpleRandomGenerator srgen(0);

      float bestM = -1.0;
      do {
        int bestAttr = -1, wins=0, attrNo=0;
        TVarList &attributes=newGen->domain->attributes.getReference();
        for(TVarList::iterator vi(attributes.begin()), ve(attributes.end()); vi!=ve; vi++, attrNo++)
          if (   !suspicious
              || !suspicious->size()
              || find(suspicious->begin(), suspicious->end(), *vi)!=suspicious->end()) {
            float thisM = measure->operator ()(attrNo, PExampleGenerator(*newGen), PDistribution(), weightID);
            if (   (!wins || (thisM <bestM)) && ((wins=1)==1)
                || (thisM==bestM) && srgen.randbool(++wins)) {
              bestAttr = attrNo; 
              bestM = thisM; 
            }
          }
        if (!wins)
          break;

        if ((bestM<=minQuality) || (int(attributes.size())>removeBut)) {
          PDomain newDomain = CLONE(TDomain, newGen->domain);
          newDomain->delVariable(attributes[bestAttr]);
          TExampleTable *newNewGen = mlnew TExampleTable(newDomain, PExampleGenerator(*newGen));
          mldelete newGen;
          newGen = newNewGen;
        }
      } while((bestM<=minQuality) || (int(newGen->domain->attributes->size())>removeBut));

      retDomain = newGen->domain;

    }
    catch (exception) {
      mldelete newGen;
      throw;
    }

    if (NRGen)
      *NRGen = newGen;
    else
      mldelete newGen;

    return retDomain;
  }
  throw 0;
}



TRemoveRedundantOneValue::TRemoveRedundantOneValue(bool anOnData)
: TRemoveRedundant(false),
  onData(anOnData)
{}

PDomain TRemoveRedundantOneValue::operator()
  (PExampleGenerator gen, PVarList suspicious, PExampleGenerator *nonRedundantResult, int weightID)
{
  PDomain newDomain = mlnew TDomain;

  if (onData) {
    TDomainDistributions distr(gen, weightID);
    TDomainDistributions::iterator di(distr.begin());
    PITERATE(TVarList, vi, gen->domain->attributes) {
      if (   suspicious && suspicious->size() && !exists(suspicious.getReference(), *vi))
        newDomain->addVariable(*vi);
      else {
        const TDiscDistribution *discdist = (*di).AS(TDiscDistribution);
        if (!discdist)
          newDomain->addVariable(*vi);
        else {
          int nonull=0;
          const_ITERATE(TDiscDistribution, dvi, *discdist)
            if ((*dvi>0) && nonull++)
              break;
          if (nonull>1)
            newDomain->addVariable(*vi);
        }
      }
      di++;
    }
  }
  else
    PITERATE(TVarList, vi, gen->domain->attributes)
      if (   suspicious && suspicious->size() && !exists(suspicious.getReference(), *vi)      // suspicious given, this one is not among them
          || !(*vi).is_derived_from(TEnumVariable)       // cannot find the number of values
          || ((*vi).AS(TEnumVariable)->noOfValues()>1)) // has enough values
        newDomain->addVariable(*vi);

  newDomain->setClass(gen->domain->classVar);
  if (nonRedundantResult)
    *nonRedundantResult=mlnew TExampleTable(newDomain, gen);

  return newDomain;
}


/*
PDomain TRemoveNonexistentValues::operator()(PExampleGenerator gen, PVarList suspicious, PExampleGenerator *nonRedundantResult, int)
{
  TVarList newVariables;
  TDomainDistributions ddist(gen);
  bool noChange = true;

  TDomainDistributions::iterator ddi(ddist.begin());
  for(TVarList::iterator vi(gen->domain->variables->begin()), ve(gen->domain->variables->end()); vi!=ve; vi++, ddi++) {
    
    if (   suspicious && suspicious->size() && !exists(suspicious.getReference(), *vi)
        || (*vi)->varType!=TValue::INTVAR)
      newVariables.push_back(*vi);

    else {
      const TDiscDistribution *discdist = (*ddi).AS(TDiscDistribution);
      if (!discdist)
        newVariables.push_back(*vi);
      else {
        TDiscDistribution::const_iterator dvi(discdist->begin()), dve(discdist->end());

        int nonull = 0;
        for(; dvi!=dve; dvi++)
          if (*dvi)
            nonull++;

        if (nonull==int((*vi).AS(TEnumVariable)->values->size()))
          newVariables.push_back(*vi);
      
        else if (nonull) {
          TEnumVariable *enewVar = mlnew TEnumVariable("R_"+(*vi)->name);
          enewVar->values = PStringList(mlnew TStringList(nonull, ""));
          PVariable newVar(enewVar);

          TClassifierByLookupTable1 *cblt = mlnew TClassifierByLookupTable1(newVar, *vi);
          int cnt = 0;
          TIdList::iterator vali = (*vi).AS(TEnumVariable)->values->begin();
          vector<TValue>::iterator lvi(cblt->lookupTable->begin());
          vector<PDistribution>::iterator ldi(cblt->distributions->begin());
          for(dvi = discdist->begin(), dve = discdist->end(); dvi!=dve; dvi++, vali++, lvi++, ldi++)
            if (*dvi) {
              enewVar->values->at(cnt) = *vali;
              *lvi = TValue(cnt);
              (*ldi)->addint(cnt, 1.0);
              cnt++;
            }

          newVar->getValueFrom = cblt;
          newVariables.push_back(newVar);
          noChange = false;
        }
      }
    }
  }

  if (noChange) {
    if (nonRedundantResult)
      *nonRedundantResult=gen;
    return gen->domain;
  }

  else {
    PDomain newDomain=mlnew TDomain(newVariables);
    if (nonRedundantResult)
      *nonRedundantResult=mlnew TExampleTable(newDomain, gen);
    return newDomain;
  }
}
*/

TRemoveUnusedValues::TRemoveUnusedValues(bool rov)
: removeOneValued(rov)
{}


PVariable TRemoveUnusedValues::operator()(PVariable var, PExampleGenerator gen, const int &weightID)
{
  TEnumVariable *evar = var.AS(TEnumVariable);
  if (!evar)
    raiseError("'%s' is not a discrete attribute", var->name);

  TDiscDistribution dist(gen, var, weightID);
  TDiscDistribution::const_iterator dvi, dve;

  int nonull = 0;
  for(dvi = dist.begin(), dve = dist.end(); dvi!=dve; dvi++)
    if (*dvi > 1e-20)
      nonull++;

  if (!nonull || (removeOneValued && (nonull==1)))
    return PVariable();

  if (nonull==int(evar->values->size()))
    return var;

  TEnumVariable *enewVar = mlnew TEnumVariable("R_"+evar->name);
  enewVar->values = PStringList(mlnew TStringList(nonull, ""));
  PVariable newVar(enewVar);

  TClassifierByLookupTable1 *cblt = mlnew TClassifierByLookupTable1(newVar, var);
  int cnt = 0;
  TIdList::iterator vali = evar->values->begin();
  vector<TValue>::iterator lvi(cblt->lookupTable->begin());
  vector<PDistribution>::iterator ldi(cblt->distributions->begin());
  for(dvi = dist.begin(), dve = dist.end(); dvi!=dve; dvi++, vali++, lvi++, ldi++)
    if (*dvi > 1e-20) {
      enewVar->values->at(cnt) = *vali;
      *lvi = TValue(cnt);
      (*ldi)->addint(cnt, 1.0);
      cnt++;
    }

  newVar->getValueFrom = cblt;

  return newVar;
}
