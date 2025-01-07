package main

import (
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
)

func main() {
	r := gin.Default()

	// Simple GET endpoint that returns "OK"
	r.GET("/ping", func(c *gin.Context) {
		c.String(http.StatusOK, "OK")
	})

	// Endpoint with delay simulation
	r.GET("/delay/:ms", func(c *gin.Context) {
		ms := c.Param("ms")
		duration, _ := time.ParseDuration(ms + "ms")
		time.Sleep(duration)
		c.String(http.StatusOK, "Delayed response")
	})

	// Endpoint that returns specified size of data
	r.GET("/size/:kb", func(c *gin.Context) {
		kbStr := c.Param("kb")
		kb, err := strconv.Atoi(kbStr)
		if err != nil {
			c.String(http.StatusBadRequest, "Invalid size")
			return
		}
		data := make([]byte, kb*1024)
		c.Data(http.StatusOK, "application/octet-stream", data)
	})

	// POST endpoint that accepts and returns JSON
	r.POST("/echo", func(c *gin.Context) {
		var body interface{}
		if err := c.BindJSON(&body); err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
		c.JSON(http.StatusOK, body)
	})

	r.Run(":8080")
}
