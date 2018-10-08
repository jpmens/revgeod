#!/bin/sh

exec curl -s http://localhost:8865/stats | jq . 
