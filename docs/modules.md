---
title: SCIRun Module Documentation
category: info
tags: module
layout: null
---

<script type="text/javascript" async
  src="https://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS_CHTML">
</script>

<link rel="stylesheet" href="css/modest.css">

<script type="text/javascript">
<!--
    function toggle_visibility(id) {
       var e = document.getElementsByName(id)[0];
       if(e.style.display == 'block')
          e.style.display = 'none';
       else
          e.style.display = 'block';
    }
//-->
</script>

<style>
div.hidden {
    display: none;
}
</style>

<a id="top"></a>

## SCIRun Modules

{% comment %}from https://gist.github.com/pepelsbey/9334494{% endcomment %}
{% capture tmp %}
  {% for page in site.pages %}
    {% if page.category == "moduledocs" %}
      {{ page.module.category }}
    {% endif %}
  {% endfor %}
{% endcapture %}

{% assign categories = tmp | split: ' ' %}
{% assign tmp = categories[0] %}

{% for cat in categories %}
  {% unless tmp contains cat %}
    {% capture tmp %}{{ tmp }} {{ cat }}{% endcapture %}
  {% endunless %}
{% endfor %}

{% assign modulecategories = tmp | split: ' ' %}

{% capture modulepages %}
  {% for cat in modulecategories %}
    ?{{ cat }}
    {% for page in site.pages %}
      {% if page.module.category == cat %}
        ${{ page.title }}#{{ page.url }}
      {% endif %}
    {% endfor %}
  {% endfor %}
{% endcapture %}

{% assign sortedpages = modulepages | strip | strip_newlines | split: '?' | sort %}

{% for pagestring in sortedpages %}
  {% assign pageitems = pagestring | split: '$' %}
  {% if pageitems[0] %}
### <a id="{{ pageitems[0] | strip }}"></a>{{ pageitems[0] }}
    {% for item in pageitems %}
      {% comment %}skip category list item (index 0){% endcomment %}
      {% if forloop.first %} {% continue %} {% endif %}
      {% assign linkitem = item | split: '#' %}
      {% assign contentId = linkitem[0] | prepend: 'id_' %}

<h4><a name="{{linkitem[0]}}" data-proofer-ignore></a><a onclick="toggle_visibility('{{ contentId }}');" style="cursor: pointer;" data-proofer-ignore> {{ linkitem[0] }} </a></h4>
      {% capture mdpath %}modules/{{linkitem[0]}}.md{% endcapture %}
      {% capture my-include %}{% include {{mdpath}} %}  {% endcapture %}
      {% assign importantPart1 = my-include | split: 'Summary' %}
      {% assign importantPart2 = importantPart1[1] %}
<div class="hidden" markdown="1" name="{{contentId}}">{{ importantPart2  }} </div>
    {% endfor %}
  {% endif %}
{% endfor %}
