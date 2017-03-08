library(httr)
library(jsonlite)
library(plyr)


# url with some information about project in Andalussia
url <- 'http://datapoint.metoffice.gov.uk/public/data/val/wxobs/all/json/3772?res=hourly&key=YOURAPIKEY'

# read url and convert to data.frame
document <- fromJSON(txt=url)


#subset to get yesterdays data
yesterday<-document$SiteRep$DV$Location$Period$Rep[[1]]
#make a list of yesterdays date to create a date column
yesterdate<-rep(document$SiteRep$DV$Location$Period$value[[1]],
                nrow(document$SiteRep$DV$Location$Period$Rep[[1]]))
#add the column
yesterday$date<-yesterdate

#repeat for todays data
today<-document$SiteRep$DV$Location$Period$Rep[[2]]
todaysdate<-rep(document$SiteRep$DV$Location$Period$value[[2]],
                nrow(document$SiteRep$DV$Location$Period$Rep[[2]]))
today$date<-todaysdate

#create DF with 24hrs data
tmp<-rbind.fill(yesterday,
           today)
#todays temperatures
#tmp$T<- as.numeric(tmp$T)
time<-as.numeric(tmp$'$')
Time <-time/60
tmp$Time<-Time
tmp$DateTime<-as.POSIXlt(paste(tmp$date,tmp$Time),format = "%Y-%m-%dZ %H")

#thingspeak API with 1440 results for 24hours
url2<-"https://api.thingspeak.com/channels/YOURCHANNELNUM/feeds.json?results=1440&api_key=YOURAPIKEY"
#convert to DF from JSON
documentthing <- fromJSON(txt=url2)
#convert datetime to correct format
documentthing$feeds$DateTime<-as.POSIXlt(documentthing$feeds$created_at, format="%Y-%m-%dT%H:%M:%SZ")


#plot the Heathrow data
plot(tmp$DateTime ,tmp$T,type = "l",col="blue",ylim = c(-4,25),
     xlab = "DateTime",ylab="Temperature (c)")

#add the house temperature data
lines(documentthing$feeds$DateTime,documentthing$feeds$field1,col="red")

legend(x="right",legend = c("Inside Temperature","Outside Temperature"), 
      lty=c(1,1),lwd=c(2.5,2.5),col=c("red","blue"))
title(main = "Inside Temperature vs Outside Temperature (Heathrow)")
