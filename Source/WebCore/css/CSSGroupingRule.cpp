/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSGroupingRule.h"

#include "CSSParser.h"
#include "CSSRuleList.h"
#include "CSSStyleSheet.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSGroupingRule::CSSGroupingRule(StyleRuleGroup& groupRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_groupRule(groupRule)
    , m_childRuleCSSOMWrappers(groupRule.childRules().size())
{
}

CSSGroupingRule::~CSSGroupingRule()
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());
    for (auto& wrapper : m_childRuleCSSOMWrappers) {
        if (wrapper)
            wrapper->setParentRule(nullptr);
    }
}

Ref<const StyleRuleGroup> CSSGroupingRule::protectedGroupRule() const
{
    return m_groupRule;
}

Ref<StyleRuleGroup> CSSGroupingRule::protectedGroupRule()
{
    return m_groupRule;
}

ExceptionOr<unsigned> CSSGroupingRule::insertRule(const String& ruleString, unsigned index)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());

    if (index > m_groupRule->childRules().size()) {
        // IndexSizeError: Raised if the specified index is not a valid insertion point.
        return Exception { ExceptionCode::IndexSizeError };
    }

    RefPtr styleSheet = parentStyleSheet();
    auto nestedContextWithCurrentRule = [&] -> CSSParserEnum::NestedContext {
        if (m_groupRule->isStyleRule()) {
            ASSERT_NOT_REACHED(); // This is handled in CSSStyleRule.
            return CSSParserEnum::NestedContextType::Style;
        }
        if (m_groupRule->isScopeRule())
            return CSSParserEnum::NestedContextType::Scope;
        // Find the context in the ancestor chain.
        return nestedContext();
    }();
    RefPtr newRule = CSSParser::parseRule(ruleString, parserContext(), styleSheet ? &styleSheet->contents() : nullptr, CSSParser::AllowedRules::ImportRules, nestedContextWithCurrentRule);
    if (!newRule) {
        // CSSNestedDeclarations parsing is allowed if there is an ancestor style rule or an ancestor scope rule.
        if (!nestedContextWithCurrentRule)
            return Exception { ExceptionCode::SyntaxError };
        newRule = CSSParser::parseNestedDeclarations(parserContext(), ruleString);
        if (!newRule)
            return Exception { ExceptionCode::SyntaxError };
    }

    if (newRule->isImportRule() || newRule->isNamespaceRule()) {
        // FIXME: an HierarchyRequestError should also be thrown for a @charset.
        // They are currently not getting parsed, resulting in a SyntaxError
        // to get raised above.

        // HierarchyRequestError: Raised if the rule cannot be inserted at the specified
        // index, e.g., if an @import rule is inserted after a standard rule set or other
        // at-rule.
        return Exception { ExceptionCode::HierarchyRequestError };
    }

    if (hasStyleRuleAncestor() && !newRule->isStyleRule() && !newRule->isGroupRule() && !newRule->isNestedDeclarationsRule())
        return Exception { ExceptionCode::HierarchyRequestError };

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_groupRule->wrapperInsertRule(index, newRule.releaseNonNull());

    m_childRuleCSSOMWrappers.insert(index, RefPtr<CSSRule>());
    return index;
}

ExceptionOr<void> CSSGroupingRule::deleteRule(unsigned index)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());

    if (index >= m_groupRule->childRules().size()) {
        // IndexSizeError: Raised if the specified index does not correspond to a
        // rule in the media rule list.
        return Exception { ExceptionCode::IndexSizeError };
    }

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_groupRule->wrapperRemoveRule(index);

    if (m_childRuleCSSOMWrappers[index])
        m_childRuleCSSOMWrappers[index]->setParentRule(nullptr);
    m_childRuleCSSOMWrappers.removeAt(index);

    return { };
}

void CSSGroupingRule::appendCSSTextForItemsInternal(StringBuilder& builder, StringBuilder& rules) const
{
    builder.append(" {"_s);
    if (rules.isEmpty()) {
        builder.append("\n}"_s);
        return;
    }

    builder.append(static_cast<StringView>(rules), "\n}"_s);
}

void CSSGroupingRule::appendCSSTextForItems(StringBuilder& builder) const
{
    StringBuilder rules;
    cssTextForRules(rules);
    appendCSSTextForItemsInternal(builder, rules);
}

void CSSGroupingRule::cssTextForRules(StringBuilder& rules) const
{
    auto& childRules = m_groupRule->childRules();
    for (unsigned index = 0; index < childRules.size(); ++index) {
        auto ruleText = item(index)->cssText();
        if (!ruleText.isEmpty())
            rules.append("\n  "_s, WTFMove(ruleText));
    }
}

void CSSGroupingRule::appendCSSTextWithReplacementURLsForItems(StringBuilder& builder, const CSS::SerializationContext& context) const
{
    StringBuilder rules;
    cssTextForRulesWithReplacementURLs(rules, context);
    appendCSSTextForItemsInternal(builder, rules);
}

void CSSGroupingRule::cssTextForRulesWithReplacementURLs(StringBuilder& rules, const CSS::SerializationContext& context) const
{
    auto& childRules = m_groupRule->childRules();
    for (unsigned index = 0; index < childRules.size(); index++) {
        auto wrappedRule = item(index);
        rules.append("\n  "_s, wrappedRule->cssText(context));
    }
}

RefPtr<StyleRuleWithNesting> CSSGroupingRule::prepareChildStyleRuleForNesting(StyleRule& styleRule)
{
    CSSStyleSheet::RuleMutationScope scope(this);
    auto& rules = m_groupRule->m_childRules;
    for (size_t i = 0 ; i < rules.size() ; i++) {
        if (rules[i].ptr() == &styleRule) {
            auto styleRuleWithNesting = StyleRuleWithNesting::create(WTFMove(styleRule));
            rules[i] = styleRuleWithNesting;
            return styleRuleWithNesting;
        }        
    }
    return { };
}

unsigned CSSGroupingRule::length() const
{ 
    return m_groupRule->childRules().size(); 
}

CSSRule* CSSGroupingRule::item(unsigned index) const
{ 
    if (index >= length())
        return nullptr;
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());
    auto& rule = m_childRuleCSSOMWrappers[index];
    if (!rule)
        rule = m_groupRule->childRules()[index]->createCSSOMWrapper(const_cast<CSSGroupingRule&>(*this));
    return rule.get();
}

CSSRuleList& CSSGroupingRule::cssRules() const
{
    if (!m_ruleListCSSOMWrapper)
        lazyInitialize(m_ruleListCSSOMWrapper, makeUniqueWithoutRefCountedCheck<LiveCSSRuleList<CSSGroupingRule>>(const_cast<CSSGroupingRule&>(*this)));
    return *m_ruleListCSSOMWrapper;
}

void CSSGroupingRule::reattach(StyleRuleBase& rule)
{
    m_groupRule = downcast<StyleRuleGroup>(rule);
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->reattach(m_groupRule->childRules()[i]);
    }
}

} // namespace WebCore
