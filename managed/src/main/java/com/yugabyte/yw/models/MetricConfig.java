// Copyright (c) YugaByte, Inc.

package com.yugabyte.yw.models;

import com.avaje.ebean.Model;
import com.avaje.ebean.annotation.DbJson;
import com.fasterxml.jackson.databind.JsonNode;
import play.libs.Json;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.Transient;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

@Entity
public class MetricConfig extends Model {
  public static class Layout {
    public static class Axis {
      public String type;
      public Map<String, String> alias = new HashMap<>();
    }

    public String title;
    public Axis xaxis;
    public Axis yaxis;
  }

  @Transient
  public String metric;
  @Transient
  public String function;
  @Transient
  public String range;
  @Transient
  public JsonNode filters;
  @Transient
  public String group_by;
  @Transient
  public JsonNode layout;

  @Id
  @Column(length = 100)
  private String config_key;
  public String getKey() { return config_key; }
  public void setKey(String key) { this.config_key = key; }

  @Column(nullable = false)
  @DbJson
  private JsonNode config;

  public void setConfig(JsonNode config) { this.config = config; }
  public MetricConfig getConfig() { return Json.fromJson(this.config, MetricConfig.class); }

  // If we have any special filter pattern, then we need to use =~ instead
  // of = in our filter condition. Special patterns include *, |, $ or +.
  static final Pattern specialFilterPattern = Pattern.compile("[*|+$]");

  public Map<String, String> getFilters() {
    if (this.getConfig().filters != null) {
      return Json.fromJson(this.getConfig().filters, Map.class);
    }
    return new HashMap<>();
  }

  public Layout getLayout() {
    if (this.getConfig().layout != null) {
      return Json.fromJson(this.getConfig().layout, Layout.class);
    }
    return new Layout();
  }

  /**
   * This method construct the prometheus queryString based on the metric config
   * if additional filters are provided, it applies those filters as well.
   * example query string:
   *  - avg(collectd_cpu_percent{cpu="system"})
   *  - rate(collectd_cpu_percent{cpu="system"}[30m])
   *  - avg(collectd_memory{memory=~"used|buffered|cached|free"}) by (memory)
   *
   * @return, a valid prometheus query string
   */
  public String getQuery(Map<String, String> additionalFilters) {
    String queryStr;
    StringBuilder query = new StringBuilder();
    MetricConfig metricConfig = getConfig();
    if (metricConfig.metric == null) {
      throw new RuntimeException("Invalid MetricConfig: metric attribute is required");
    } else if (!metricConfig.metric.equals("multi-metric")) {
      // We will use a special metric key for any metric where we have to query
      // multiple metrics in one. In those scenarios we would use the filter
      // condition with wildcard to narrow the metrics. ex: network_packets
      query.append(metricConfig.metric);
    }

    Map<String, String> filters = this.getFilters();
    // If we have additional filters, we add them
    if (!additionalFilters.isEmpty()) {
      filters.putAll(additionalFilters);
    }

    if (!filters.isEmpty()) {
      query.append(filtersToString(filters));
    }

    // Range is applicable only when we have functions
    // TODO: also need to add a check, since range is applicable for only certain functions
    if (metricConfig.range != null && metricConfig.function != null) {
      query.append(String.format("[%s]", metricConfig.range));
    }

    queryStr = query.toString();

    if (metricConfig.function != null) {
      /* We have added special way to represent multiple functions that we want to
         do, we pipe delimit those, but they follow an order.
         Scenario 1:
           function: rate|avg,
           query str: avg(rate(metric{memory="used"}[30m]))
         Scenario 2:
           function: rate
           query str: rate(metric{memory="used"}[30m]). */
      if (metricConfig.function.contains("|")) {
        // We need to split the multiple functions and form the query string
        String[] functions = metricConfig.function.split("\\|");
        for (String functionName : functions) {
          queryStr = String.format("%s(%s)", functionName, queryStr);
        }
      } else {
        queryStr = String.format("%s(%s)", metricConfig.function, queryStr);
      }
    }

    if (getConfig().group_by != null) {
      queryStr = String.format("%s by (%s)", queryStr, metricConfig.group_by);
    }

    return queryStr;
  }

  /**
   * filtersToString method converts a map to a string with quotes around the value.
   * The reason we have to do this way is because prometheus expects the json
   * key to have no quote, and just value should have double quotes.
   * @param filters is map<String, String>
   * @return String representation of the map
   * ex:
   *  {memory="used", extra="1"}
   *  {memory="used"}
   *  {type=~"iostat_write_count|iostat_read_count"}
   */
  private String filtersToString(Map<String, String> filters) {
    StringBuilder filterStr = new StringBuilder();
    String prefix = "{";
    for(Map.Entry<String, String> filter : filters.entrySet()) {
      filterStr.append(prefix);
      if (specialFilterPattern.matcher(filter.getValue()).find()) {
        filterStr.append(filter.getKey() + "=~\"" + filter.getValue() + "\"");
      } else {
        filterStr.append(filter.getKey() + "=\"" + filter.getValue() + "\"");
      }
      prefix = ", ";
    }
    filterStr.append("}");
    return filterStr.toString();
  }

  public static final Find<String, MetricConfig> find = new Find<String, MetricConfig>() {};

  /**
   * returns metric config for the given key
   * @param configKey
   * @return MetricConfig
   */
  public static MetricConfig get(String configKey) {
    return MetricConfig.find.byId(configKey);
  }

  /**
   * Create a new instance of metric config for given config key and config data
   * @param configKey
   * @param configData
   * @return returns a instance of MetricConfig
   */
  public static MetricConfig create(String configKey, JsonNode configData) {
    MetricConfig metricConfig = new MetricConfig();
    metricConfig.setKey(configKey);
    metricConfig.setConfig(Json.toJson(configData));
    return metricConfig;
  }

  /**
   * Loads the configs into the db, if the config already exists it would update that
   * if not it will create new config.
   * @param configs
   */
  public static void loadConfig(Map<String, Object> configs) {
    List<String> currentConfigs = MetricConfig.find.all().stream()
      .map(MetricConfig::getKey).collect(Collectors.toList());

    for(Map.Entry<String, Object> configData : configs.entrySet()) {
      MetricConfig metricConfig = MetricConfig.create(configData.getKey(),
                                                      Json.toJson(configData.getValue()));
      // Check if the config already exists if so, let's update it or else,
      // we will create new one.
      if (currentConfigs.contains(metricConfig.getKey())) {
        metricConfig.update();
      } else {
        metricConfig.save();
      }
    }
  }
}
