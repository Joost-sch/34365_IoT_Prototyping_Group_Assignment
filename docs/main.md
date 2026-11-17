# How the overall device works

1. Use interrupt service routine based on acceloremeter movement
2. When bottle is picked up or detecting movement of drinking, start all modules
3. After done drinking (we detect from pitch/roll be almost vertical and constant), pressure sensor measure the final weight
4. Store the final weight in persistent memory for future measurement
5. Calculate the difference between final weight and previous weight and LoraWan send to the database
6. Database receive, add new row of timestamp + amount of water drank
7. Database re evaluate the percentage to goal and send the data back to a different pro mini
8. Pro mini (2) received and update the eink display