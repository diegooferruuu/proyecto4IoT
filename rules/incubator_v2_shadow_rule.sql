SELECT state.reported.temperatureState AS temperatureState, topic(3) AS thing_name
FROM '$aws/things/+/shadow/update/accepted' 
WHERE substring(topic(3), 0, 9) = 'incubator' AND state.reported.temperatureState >= 0 AND state.reported.temperatureState <= 2 
AND state.reported.incubatorMode = 1