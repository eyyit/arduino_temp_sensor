import test from 'node:test';
import assert from 'node:assert/strict';
import {health,ingest,duration,pretty,normalize} from '../web/sensor-ui.mjs';
const fresh = () => ({connected:true,values:{sensor_sample_age:2,sensor_sample_fresh:1,temperature_fahrenheit:68,temperature_celsius:20,humidity:45,pressure:1020},received:{sensor_sample_age:1000,temperature_fahrenheit:1000,temperature_celsius:1000,humidity:1000,pressure:1000}});
test('fresh frame becomes stale as time passes without new events',() => {
  assert.equal(health(fresh(),2000).fresh,true); assert.equal(health(fresh(),15000).fresh,false);
});
test('disconnected browser never presents old climate values as fresh',() => {
  const model=fresh(); model.connected=false; assert.equal(health(model,1000).fresh,false);
});
test('initial or partial frame is never called fresh',() => {
  assert.equal(health({connected:true,values:{},received:{}},1000).fresh,false);
  const model=fresh(); delete model.values.humidity; assert.equal(health(model,1000).fresh,false);
});
test('explicit sensor failure invalidates an otherwise numeric frame',() => {
  const model=fresh(); model.values.sensor_sample_fresh=0; assert.equal(health(model,1000).fresh,false);
});
test('null, NaN and numeric strings are unavailable, not zero',() => {
  for(const value of [null,NaN,Infinity,'20']) {
    const model=fresh(); ingest(model,{id:'sensor/Temperature Celsius',value},1000);
    assert.equal(model.values.temperature_celsius,null); assert.equal(health(model,1000).fresh,false);
  }
});
test('actual pinned API identifiers map to UI keys',() => {
  assert.equal(normalize('sensor/Heap Fragmentation Percent'),'heap_fragmentation_percent');
  assert.equal(normalize('sensor/Temperature Fahrenheit'),'temperature_fahrenheit');
});
test('non-sensor and malformed messages do not change state',() => {
  const model=fresh(), before=JSON.stringify(model);
  for(const event of [null,{}, {id:'switch/Example',value:1}]) assert.equal(ingest(model,event,1000),false);
  assert.equal(JSON.stringify(model),before);
});
test('each climate channel must have a recent update',() => {
  const model=fresh(); model.received.humidity=-20000; assert.equal(health(model,1000).fresh,false);
});
test('value formatting is consistently two decimals and handles unavailable values',() => {
  assert.equal(pretty(1.2,'%'),'1.20 %'); assert.equal(pretty(null),'—');
  assert.equal(duration(90061),'1d 1h'); assert.equal(duration(3660),'1h 1m'); assert.equal(duration(NaN),'—');
});
